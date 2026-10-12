# Programming Assignment 02: Re-engineering the `ls` Utility — Report

**Course:** Operating Systems  
**Instructor:** Muhammad Arif Butt, PhD  
**Student Roll Number:** BSDSF24A051  
**Repository:** `BSDSF24A051-OS-AO2`

---

## Table of Contents
1. [Feature 2: Long Listing Format (-l)](#feature-2-long-listing-format--l)
2. [Feature 3: Column Display (Down Then Across)](#feature-3-column-display-down-then-across)
3. [Feature 4: Horizontal Column Display (-x)](#feature-4-horizontal-column-display--x)
4. [Feature 5: Alphabetical Sorting](#feature-5-alphabetical-sorting)
5. [Feature 6: Colorized Output Based on File Type](#feature-6-colorized-output-based-on-file-type)
6. [Feature 7: Recursive Listing (-R)](#feature-7-recursive-listing--r)

---

## Feature 2: Long Listing Format (-l)

### Q1: What is the crucial difference between the `stat()` and `lstat()` system calls? In the context of the `ls` command, when is it more appropriate to use `lstat()`?

**Answer:**
* **`stat()`**: When given the pathname of a symbolic link, `stat()` follows (dereferences) the link and retrieves the metadata of the target file that the link points to. If the target file does not exist (a dangling symlink), `stat()` fails and returns `-1`.
* **`lstat()`**: Unlike `stat()`, `lstat()` does **not** follow the symbolic link. Instead, it retrieves metadata about the symbolic link file itself (such as its link size, symlink permissions, and ownership).
* **Appropriateness in `ls`**: For a directory listing utility like `ls`, using `lstat()` is essential because `ls` must report on the files present directly within the directory being inspected. If `stat()` were used:
  1. A symbolic link would appear with the attributes and type of its target rather than showing as a link (`l` prefix, symlink permissions `lrwxrwxrwx`).
  2. Dangling symbolic links whose targets are missing or unreadable would produce errors during listing even though the link itself exists and is valid in the directory.
  3. Symlinks pointing to directories could inadvertently trigger directory-specific actions or infinite loops during traversal.
Therefore, `lstat()` is the correct and standard choice for `ls`.

---

### Q2: The `st_mode` field in `struct stat` is an integer that contains both the file type (e.g., regular file, directory) and the permission bits. Explain how you can use bitwise operators (like `&`) and predefined macros (like `S_IFDIR` or `S_IRUSR`) to extract this information.

**Answer:**
`st_mode` is a 16-bit integer encoding multiple pieces of file metadata into distinct bitfields:
1. **File Type bits (high bits):**
   The file type is stored in bits masked by `S_IFMT` (octal `0170000`). To extract the file type using bitwise operators, we use the bitwise AND operator (`&`) with the mask, then compare against file type constants:
   ```c
   if ((st.st_mode & S_IFMT) == S_IFDIR) { /* Directory */ }
   if ((st.st_mode & S_IFMT) == S_IFREG) { /* Regular File */ }
   if ((st.st_mode & S_IFMT) == S_IFLNK) { /* Symbolic Link */ }
   ```
   POSIX provides convenience macros that perform this operation directly:
   * `S_ISDIR(mode)` evaluates `((mode & S_IFMT) == S_IFDIR)`
   * `S_ISREG(mode)` evaluates `((mode & S_IFMT) == S_IFREG)`
   * `S_ISLNK(mode)` evaluates `((mode & S_IFMT) == S_IFLNK)`
   * `S_ISCHR(mode)`, `S_ISBLK(mode)`, `S_ISFIFO(mode)`, `S_ISSOCK(mode)`

2. **File Permission bits (lower 9 bits + special bits):**
   Individual permission bits are extracted using the bitwise AND (`&`) operator with predefined constants:
   * **User (Owner):** `mode & S_IRUSR` (read, `0400`), `mode & S_IWUSR` (write, `0200`), `mode & S_IXUSR` (execute, `0100`).
   * **Group:** `mode & S_IRGRP` (read, `0040`), `mode & S_IWGRP` (write, `0020`), `mode & S_IXGRP` (execute, `0010`).
   * **Others:** `mode & S_IROTH` (read, `0004`), `mode & S_IWOTH` (write, `0002`), `mode & S_IXOTH` (execute, `0001`).
   * **Special bits:** `S_ISUID` (`04000`, Set-User-ID), `S_ISGID` (`02000`, Set-Group-ID), `S_ISVTX` (`01000`, Sticky bit).
   
In our code:
```c
putchar((mode & S_IRUSR) ? 'r' : '-');
putchar((mode & S_IWUSR) ? 'w' : '-');
putchar((mode & S_ISUID) ? 's' : (mode & S_IXUSR) ? 'x' : '-');
```

---

## Feature 3: Column Display (Down Then Across)

### Q3: Explain the general logic for printing items in a "down then across" columnar format. Why is a simple single loop through the list of filenames insufficient for this task?

**Answer:**
* **General Logic:**
  Standard terminals print output line by line (row by row), from top to bottom. In a "down then across" layout with `N` items and `C` columns:
  1. Determine the number of rows needed using ceiling division:
     ```
     num_rows = ceil(N / C) = (N + C - 1) / C
     ```
  2. For any given row `r` (where `0 <= r < num_rows`) and column `c` (where `0 <= c < C`), the item index in our sorted flat array is:
     ```
     index = r + c * num_rows
     ```
  3. We execute an outer loop iterating over each row `r` from `0` to `num_rows - 1`, and an inner loop iterating over column `c` from `0` to `C - 1`. If `index < N`, we print the item formatted with padding up to the column width. When the row finishes, we emit a newline `\n`.
* **Why a single loop is insufficient:**
  Because terminal printing streams text horizontally across a line before moving to the next line. If you used a simple sequential loop from `i = 0` to `N - 1`, items would naturally be placed horizontally (row-major: item 0, item 1, item 2 on the first row). To make items flow vertically down column 1 before starting column 2, the printer must access non-contiguous indices across the array (`0`, `num_rows`, `2 * num_rows`, `...`) during the first line output. This requires pre-calculating `num_rows` and indexing by `r + c * num_rows`.

---

### Q4: What is the purpose of the `ioctl` system call in this context? What would be the limitations of your program if you only used a fixed-width fallback (e.g., 80 columns) instead of detecting the terminal size?

**Answer:**
* **Purpose of `ioctl`:**
  The `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)` system call queries device parameters of the standard output terminal. The request `TIOCGWINSZ` ("terminal I/O get window size") fills a `struct winsize` with the terminal's actual physical dimensions in characters (`ws_col` for columns, `ws_row` for rows).
* **Limitations of a fixed-width fallback (e.g., 80 columns):**
  1. **Wasted Screen Space:** On modern high-resolution displays or maximized terminal windows (often 160–240 columns wide), a hard-coded 80-column limit wraps columns prematurely, leaving large empty spaces on the right side of the screen and producing unnecessarily long vertical scroll lists.
  2. **Ugly Line Wrapping:** On narrower terminals, side-by-side terminal splits, or mobile/SSH sessions (e.g., 60 columns wide), assuming 80 columns causes text to overflow the visible width, creating broken, jagged lines that ruin columnar alignment.
Dynamically detecting width via `ioctl` guarantees that the utility always maximizes column density while preserving clean layout.

---

## Feature 4: Horizontal Column Display (-x Option)

### Q5: Compare the implementation complexity of the "down then across" (vertical) printing logic versus the "across" (horizontal) printing logic. Which one requires more pre-calculation and why?

**Answer:**
* **Pre-calculation Comparison:**
  The "down then across" (vertical) mode requires significantly more pre-calculation.
  * **"Down then across" (Vertical):** Requires determining the column width, computing the total number of columns that fit, computing the exact number of rows (`num_rows = (count + num_cols - 1) / num_cols`), and computing non-contiguous array indices `r + c * num_rows` for every cell. All filenames must be completely counted and organized into a 2D mathematical grid before a single character can be printed.
  * **"Across" (Horizontal):** Operates sequentially (row-major order). It only needs the column width and terminal width. It iterates from index `0` to `N - 1` in simple linear order, tracking current cursor position `pos`. When `pos + col_width > term_width`, it emits `\n` and resets `pos = 0`.
* **Conclusion:**
  "Down then across" has higher implementation complexity and strictly requires pre-calculating grid geometry, whereas horizontal layout directly mirrors standard terminal streaming order.

---

### Q6: Describe the strategy you used in your code to manage the different display modes (-l, -x, and default). How did your program decide which function to call for printing?

**Answer:**
* **State Management Strategy:**
  We defined display mode constants and an integer state variable:
  ```c
  #define MODE_DEFAULT    0   /* Down-then-across columns */
  #define MODE_LONG       1   /* -l Long listing format */
  #define MODE_HORIZONTAL 2   /* -x Horizontal (left-to-right) columns */

  static int display_mode = MODE_DEFAULT;
  ```
* **Command-Line Parsing:**
  In `main()`, `getopt(argc, argv, "lxR")` inspects options:
  * When `-l` is encountered, `display_mode = MODE_LONG;`
  * When `-x` is encountered, `display_mode = MODE_HORIZONTAL;`
  * When `-R` is encountered, `recursive_flag = 1;`
* **Dispatch Strategy:**
  Inside `do_ls(dir)`:
  1. It reads and alphabetically sorts all entries in the directory into dynamic array `names`.
  2. It evaluates `display_mode` in a clean dispatch conditional:
     ```c
     if (display_mode == MODE_LONG)
         do_ls_long(dir, names, count);
     else if (display_mode == MODE_HORIZONTAL)
         print_horizontal(dir, names, count);
     else
         print_columns(dir, names, count);
     ```
  3. If `recursive_flag` is set, it then iterates through `names` to recurse into subdirectories.
  4. Finally, it frees the allocated memory via `free_names(names, count)`.

---

## Feature 5: Alphabetical Sorting

### Q7: Why is it necessary to read all directory entries into memory before you can sort them? What are the potential drawbacks of this approach for directories containing millions of files?

**Answer:**
* **Why entries must be read into memory:**
  The `readdir()` system call reads directory entries sequentially directly from the underlying file system inode/directory tables. Filesystems (like ext4) store and return entries according to their internal structure (such as htree hash order or disk allocation order), which is generally unsorted. Because standard sorting algorithms (such as QuickSort) require random access and comparisons across all elements to establish global order, all candidate names must be loaded into memory before any sorting comparison can be completed.
* **Potential Drawbacks for Millions of Files:**
  1. **High Memory Overhead (RAM exhaustion):** Storing millions of filename strings and pointer arrays requires hundreds of megabytes or gigabytes of heap memory. In resource-constrained environments, this can trigger out-of-memory (OOM) errors.
  2. **Latency / Delay to First Output:** The user sees no output until the entire directory has been read, allocated, and sorted. For millions of entries, this causes noticeable latency compared to streaming un-sorted output.
  3. **Alternative Architecture for Giant Datasets:** Production systems handling massive directories may use external merge sorts, B-trees, or streaming pagination (`--sort=none`) to avoid loading all items into memory at once.

---

### Q8: Explain the purpose and signature of the comparison function required by `qsort()`. How does it work, and why must it take `const void *` arguments?

**Answer:**
* **Signature and Purpose:**
  ```c
  int compare_str(const void *a, const void *b);
  ```
  `qsort()` is a generic sorting function in the C standard library (`<stdlib.h>`). It does not have built-in knowledge of what data type is being sorted. It relies on the comparison function to report the relative ordering of any two elements in the array.
* **Why `const void *` arguments are required:**
  C achieves polymorphism for generic algorithms using `void *`. A `const void *` can point to any data type without type-casting warnings, and the `const` qualifier guarantees that `qsort()` will not modify the elements being compared.
* **How it works with an array of strings (`char **`):**
  When `qsort()` is called on an array of strings `char *names[]`, each element of the array is a `char *`. Therefore, `qsort()` passes pointers to elements—which are pointers to `char *` (`char **`):
  ```c
  int compare_str(const void *a, const void *b)
  {
      const char *sa = *(const char **)a;
      const char *sb = *(const char **)b;
      return strcmp(sa, sb);
  }
  ```
  * Returns `< 0` if `sa` precedes `sb` alphabetically.
  * Returns `0` if `sa` equals `sb`.
  * Returns `> 0` if `sa` follows `sb` alphabetically.

---

## Feature 6: Colorized Output Based on File Type

### Q9: How do ANSI escape codes work to produce color in a standard Linux terminal? Show the specific code sequence for printing text in green.

**Answer:**
* **How ANSI Escape Codes Work:**
  Linux terminal emulators implement the ANSI/ECMA-48 text formatting standard. When a terminal emulator encounters the escape character (ASCII 27 / octal `\033` / hex `\x1B`) followed by a left square bracket `[`, it interprets the subsequent bytes as a Control Sequence Introducer (CSI) rather than literal text. The sequence specifies display attributes such as bold, underline, foreground color, and background color, terminated by the letter `m` (Select Graphic Rendition, SGR).
* **Code Sequence for Green Text:**
  * Start color sequence: `\033[0;32m` (where `0` is normal weight and `32` is green foreground).
  * Reset sequence: `\033[0m` (resets all colors and attributes back to default terminal settings).
  * Example in C:
    ```c
    printf("\033[0;32m%s\033[0m", filename);
    ```

---

### Q10: To color an executable file, you need to check its permission bits. Explain which bits in the `st_mode` field you need to check to determine if a file is executable by the owner, group, or others.

**Answer:**
To determine if a regular file is executable, we check whether any of the execute permission bits are set in `st_mode`:
1. **Owner (User) Execute Bit:** `S_IXUSR` (`0100` octal / bit 6) — executable by file owner.
2. **Group Execute Bit:** `S_IXGRP` (`0010` octal / bit 3) — executable by members of the file's group.
3. **Others Execute Bit:** `S_IXOTH` (`0001` octal / bit 0) — executable by anyone else.

In our code:
```c
if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
    printf(COLOR_GREEN "%s" COLOR_RESET, name);
}
```
We also verify `S_ISREG(mode)` so that directories (which normally have `x` bits set to allow directory traversal) are colored blue as directories rather than green as executables.

---

## Feature 7: Recursive Listing (-R)

### Q11: In a recursive function, what is a "base case"? In the context of your recursive `ls`, what is the base case that stops the recursion from continuing forever?

**Answer:**
* **Base Case Definition:**
  In recursion, a "base case" is a condition under which the function terminates its recursive chain and returns directly without making further recursive calls. Without a base case, recursion continues indefinitely until stack memory is exhausted (causing a stack overflow / segfault).
* **Base Case in Recursive `ls`:**
  1. **Leaf Directories (No Subdirectories):** When `do_ls` inspects a directory and finds no directory entries (or only regular files, symlinks, and special files), the loop over entries finds zero items where `S_ISDIR(st.st_mode)` is true. No recursive calls are made, and the function returns.
  2. **Exclusion of `.` and `..`:** Crucially, the directory entries `.` (current directory) and `..` (parent directory) are explicitly skipped during reading. If they were visited, recursion would enter an infinite cyclic loop (`dir/././.` or `dir/../dir`).
  3. **Unopenable Directories:** If `opendir(dir)` fails (e.g., due to permission denied `EACCES`), `do_ls` prints an error and returns immediately, halting recursive descent along that subtree.

---

### Q12: Explain why it is essential to construct a full path (e.g., `"parent_dir/subdir"`) before making a recursive call. What would happen if you simply called `do_ls("subdir")` from within the `do_ls("parent_dir")` function call?

**Answer:**
* **Why constructing the full path is essential:**
  The current working directory (CWD) of the running process remains set to the directory from which the command was initially executed in the shell (unless `chdir()` is invoked).
* **What would happen if `do_ls("subdir")` was called directly:**
  If the process started in `/home/user` and is currently inspecting `parent_dir`, calling `do_ls("subdir")` causes `opendir("subdir")` and `lstat("subdir")` to resolve `"subdir"` relative to the process's CWD (`/home/user/subdir`), **not** `/home/user/parent_dir/subdir`.
  * Because `/home/user/subdir` does not exist, `opendir()` fails with `ENOENT` (No such file or directory).
  * If a different directory named `subdir` happens to exist in `/home/user`, the program would mistakenly list the wrong directory or enter an unintended infinite loop.
* **Implementation Solution:**
  By joining the parent directory path with the child entry name (`parent_dir/subdir`), every system call (`lstat`, `opendir`) operates on the unambiguous relative or absolute path from the process CWD.

---

## Summary of Completed Releases

| Version | Tag | Release Title | Assets Attached |
|:-------:|:---:|:--------------|:---------------:|
| 1.1.0 | `v1.1.0` | `Version 1.1.0: Complete Long Listing Format` | `ls` |
| 1.2.0 | `v1.2.0` | `Version 1.2.0: Column Display` | `ls` |
| 1.3.0 | `v1.3.0` | `Version 1.3.0: Horizontal Column Display (-x)` | `ls` |
| 1.4.0 | `v1.4.0` | `Version 1.4.0: Alphabetical Sort` | `ls` |
| 1.5.0 | `v1.5.0` | `Version 1.5.0: Colorized Output` | `ls` |
| 1.6.0 | `v1.6.0` | `Version 1.6.0: Recursive Listing (-R)` | `ls` |

All features, tests, tags, branches, and GitHub releases are complete and verified.
