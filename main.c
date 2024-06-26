#include <dirent.h>  // Directory
#include <fcntl.h>   // FD Manipulate (Probably won't need here)
#include <limits.h>  // INT_MAX, etc...
#include <locale.h>  // Locale
#include <magic.h>
#include <ncurses.h>  // Curses (UI)
#include <pwd.h>      // get uid, username n stuff
#include <stdio.h>    //	std i/o
#include <stdlib.h>   //	std libs
#include <string.h>   //	string opr
#include <strings.h>
#include <sys/stat.h>   // system stats
#include <sys/types.h>  //	sys types
#include <sys/wait.h>   //	child_process report kinda
#include <unistd.h>     //	POSIX definitions

#include "config.h"

#define isDir(mode) (S_ISDIR(mode))

void init_curses() {
  initscr();
  noecho();
  curs_set(0);
  start_color();
  init_pair(1, DIR_COLOR, 0);
  init_pair(3, STATUS_SELECTED_COLOR, 0);
}

// char /*current_directory_->cwd[1000],*/ *parent_dir;
struct stat file_stats;
WINDOW *current_win, *info_win, *path_win;
int selection, maxx, maxy, len = 0, start = 0;
size_t total_files = 0;
directory_t *current_directory_ = NULL;

void init() {
  current_directory_ = (directory_t *)malloc(sizeof(directory_t));
  if (current_directory_ == NULL) {
    printf("Error Occured.\n");
    exit(0);
  }
}

void init_windows() {
  current_win = newwin(maxy, maxx / 2, 0, 0);
  refresh();
  path_win = newwin(2, maxx, maxy, 0);
  refresh();
  info_win = newwin(maxy, maxx / 2, 0, maxx / 2);
  refresh();
  keypad(current_win, TRUE);
}

void refreshWindows() {
  box(current_win, '|', '-');
  box(info_win, '|', '-');
  wrefresh(current_win);
  wrefresh(path_win);
  wrefresh(info_win);
}

int get_no_files_in_directory(char *directory) {
  int len = 0;
  DIR *dir_;
  struct dirent *dir_entry;

  dir_ = opendir(directory);
  if (dir_ == NULL) {
    return -1;
  }

  while ((dir_entry = readdir(dir_)) != NULL) {
    // Skip .
    if (strcmp(dir_entry->d_name, ".") != 0) {
      len++;
    }
  }
  closedir(dir_);
  return len;
}

int get_files(char *directory, char *target[]) {
  int i = 0;
  DIR *dir_;
  struct dirent *dir_entry;

  dir_ = opendir(directory);
  if (dir_ == NULL) {
    return -1;
  }

  while ((dir_entry = readdir(dir_)) != NULL) {
    // Skip .
    if (strcmp(dir_entry->d_name, ".") != 0) {
      target[i++] = strdup(dir_entry->d_name);
    }
  }
  closedir(dir_);
  return 1;
}

void scroll_up() {
  selection--;
  selection = (selection < 0) ? 0 : selection;
  // Scrolling
  if (len >= maxy - 1)
    if (selection <= start + maxy / 2) {
      if (start == 0)
        wclear(current_win);
      else {
        start--;
        wclear(current_win);
      }
    }
}

void scroll_down() {
  selection++;
  selection = (selection > len - 1) ? len - 1 : selection;
  if (len >= maxy - 1)
    if (selection - 1 > maxy / 2) {
      if (start + maxy - 2 != len) {
        start++;
        wclear(current_win);
      }
    }
}

// void compare(const void *a, const void *b) {
//   return strcasecmp(*(char **)a, *(char **)b);
// }
void sort(char *files_[], int n) {
  char temp[1000];
  for (int i = 0; i < n - 1; i++) {
    for (int j = i + 1; j < n; j++) {
      if (strcmp(files_[i], files_[j]) > 0) {
        strcpy(temp, files_[i]);
        strcpy(files_[i], files_[j]);
        strcpy(files_[j], temp);
        // free(temp)
      }
    }
  }
}

int check_text(char *path) {
  FILE *ptr;
  ptr = fopen(path, "r");
  int c;
  while ((c = fgetc(ptr)) != EOF) {
    if (c < 0 || c > 127) {
      fclose(ptr);
      return 0;
    }
  }
  fclose(ptr);
  return 1;
}

void read_(char *path) {
  // char temp[1000];
  // snprintf(temp, 1000, "%s%s", current_directory_->cwd, files[selection]);
  unsigned char buffer[256];
  int ch;
  wclear(current_win);
  wclear(info_win);
  wresize(current_win, maxy, maxx);

  // wrefresh(current_win);
  FILE *ptr;
  printf("%s\n", path);
  ptr = fopen(path, "rb");
  if (ptr == NULL) {
    perror("Error");
  }
  int t = 2, pos = 0, lines = 0, count;

  do {
    wmove(current_win, 1, 2);
    wprintw(current_win, "Press \"E\" to Exit (Caps Lock off)");
    // PRINT:
    if (check_text(path)) {
      count = 0;
      while (fgets(buffer, sizeof(buffer), ptr)) {
        // fread(&buffer, sizeof(unsigned char), maxx, ptr);
        if (count < pos) {
          count++;
          continue;
        }
        wmove(current_win, ++t, 1);
        wprintw(current_win, "%.*s", maxx - 2, buffer);

        // if (t - pos == maxy - 2)
        //   break;
      }
    } else {
      while (!feof(ptr)) {
        fread(&buffer, sizeof(unsigned char), maxx - 2, ptr);
        wmove(current_win, ++t, 1);
        for (int i = 0; i < maxx - 2; i += 2) {
          wprintw(current_win, "%02x%02x ", (unsigned int)buffer[i],
                  (unsigned int)buffer[i + 1]);
        }
      }
    }
    box(current_win, '|', '-');
    wrefresh(current_win);
    ch = wgetch(current_win);
    wclear(current_win);
    switch (ch) {
      case 259:
        pos = pos == 0 ? 0 : pos - 1;
        // printf("%d\n", pos);
        break;
      case 258:
        // pos = pos == lines ? lines : pos + 1;
        pos++;
        // printf("%d\n", pos);
        break;
    }
  } while (ch != 'e');

  endwin();
}

void rename_file(char *files[]) {
  char new_name[100];
  int i = 0, c;
  wclear(path_win);
  wmove(path_win, 1, 0);
  while ((c = wgetch(path_win)) != '\n') {
    if (c == 127 || c == 8) {
      new_name[--i] = '\0';
    } else {
      new_name[i++] = c;
      new_name[i] = '\0';
    }
    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "%s", new_name);
  }
  c = ' ';
LOOP:
  wclear(path_win);
  wmove(path_win, 1, 0);
  wprintw(path_win, "%s (y/n) %c", new_name, c);
  c = wgetch(path_win);
  char *a, *b;
  switch (c) {
    case 'y':
    case 'Y':
      a = strdup(current_directory_->cwd);
      b = strdup(current_directory_->cwd);
      strcat(a, files[selection]);
      strcat(b, new_name);
      rename(a, b);
      break;
    case 'n':
    case 'N':
      break;
    default:
      goto LOOP;
      break;
  }
}

char *get_parent_directory(char *cwd) {
  char *a;
  a = strdup(cwd);
  int i = strlen(a) - 1;
  while (a[--i] != '/')
    ;
  a[++i] = '\0';
  return a;
}

void delete_(char *files[]) {
  char curr_path[1000];
  snprintf(curr_path, sizeof(curr_path), "%s%s", current_directory_->cwd,
           files[selection]);
  remove(curr_path);
}

void delete_file(char *files[]) {
  int c;
  wclear(path_win);
  wmove(path_win, 1, 0);
  wprintw(path_win, "Are you sure to delete? (y/n)");

LOOP_:
  c = wgetch(path_win);
  wclear(path_win);
  wmove(path_win, 1, 0);
  wprintw(path_win, "Are you sure to delete? (y/n) %c", c);
  switch (c) {
    case 'y':
    case 'Y':
      delete_(files);
      break;
    case 'n':
    case 'N':
      break;
    default:
      goto LOOP_;
      break;
  }
}

void copy_files(char *files[]) {
  char new_path[1000];
  int i = 0, c;
  wclear(path_win);
  wmove(path_win, 1, 0);
  while ((c = wgetch(path_win)) != '\n') {
    if (c == 127 || c == 8) {
      new_path[--i] = '\0';
      i = i < 0 ? 0 : i;
    } else {
      new_path[i++] = c;
      new_path[i] = '\0';
    }
    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "%s", new_path);
  }
  FILE *new_file, *old_file;
  strcat(new_path, files[selection]);
  char curr_path[1000];
  snprintf(curr_path, sizeof(curr_path), "%s%s", current_directory_->cwd,
           files[selection]);
  old_file = fopen(curr_path, "r");
  new_file = fopen(new_path, "a");
  wmove(current_win, 10, 10);
  wprintw(current_win, "%.*s,%s", maxx, curr_path, new_path);
  printf("%s %s", curr_path, new_path);
  while ((c = fgetc(old_file)) != EOF) {
    fprintf(new_file, "%c", c);
  }
  fclose(old_file);
  fclose(new_file);
}

void move_file(char *files[]) {
  copy_files(files);
  delete_(files);
}

void handle_enter(char *files[]) {
  char *temp, *a;
  a = strdup(current_directory_->cwd);
  endwin();
  if (strcmp(files[selection], "..") == 0) {
    start = 0;
    selection = 0;
    strcpy(current_directory_->cwd, current_directory_->parent_dir);
    current_directory_->parent_dir =
        strdup(get_parent_directory(current_directory_->cwd));
  } else {
    temp = malloc(strlen(files[selection]) + 1);
    snprintf(temp, strlen(files[selection]) + 2, "%s", files[selection]);
    strcat(a, temp);
    stat(a, &file_stats);
    if (isDir(file_stats.st_mode)) {
      start = 0;
      selection = 0;
      current_directory_->parent_dir = strdup(current_directory_->cwd);
      strcat(current_directory_->cwd, temp);
      strcat(current_directory_->cwd, "/");
    } else {
      // printf("Entered\n");
      char temp_[1000];
      // printf("%s%s", current_directory_->cwd, files[selection]);
      snprintf(temp_, sizeof(temp_), "%s%s", current_directory_->cwd,
               files[selection]);

      // snprintf(s, sizeof(s), "%s %s", "xdg-open", temp_);
      // printf("%s", temp_);
      read_(temp_);
      // system(s);
    }
  }
  refresh();
  // wmove(info_win, 14, 0);
  // wprintw(info_win, "%.*s", maxx, temp);
  // refreshWindows();
  // sleep(5);
}

float get_recursive_size_directory(char *path) {
  float directory_size = 0;
  DIR *dir_ = NULL;
  struct dirent *dir_entry;
  struct stat file_stat;
  char temp_path[1000];
  dir_ = opendir(path);
  if (dir_ == NULL) {
    perror(path);
    exit(0);
    return -1;
  }
  // wmove(info_win, 10, 12);
  while ((dir_entry = readdir(dir_)) != NULL) {
    if (strcmp(dir_entry->d_name, ".") != 0 &&
        strcmp(dir_entry->d_name, "..") != 0) {
      snprintf(temp_path, sizeof(temp_path), "%s/%s", path, dir_entry->d_name);

      if (dir_entry->d_type != DT_DIR) {
        stat(temp_path, &file_stat);
        total_files++;
        directory_size += (float)(file_stat.st_size) / (float)1024;
        // wprintw(info_win, "%s %f\n", dir_entry->d_name, directory_size);
        // wrefresh(info_win);
      } else {
        total_files++;
        directory_size += (float)4;
        directory_size += get_recursive_size_directory(temp_path);
      }
    }
  }
  closedir(dir_);
  return directory_size;
}

void change_permissions(char *files[]) {
  char temp_address[1000];
  if (strcmp(files[selection], "..") != 0) {
    snprintf(temp_address, sizeof(temp_address), "%s%s",
             current_directory_->cwd, files[selection]);

    // Move the cursor to the bottom of the window
    wmove(info_win, getmaxy(info_win) - 5, 1);

    int permissions;
    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "Select permission option:  ");

  LOOP_:
    int option;
    if (wscanw(path_win, "%d", &option) != 1) {
      wprintw(path_win, "Invalid input. Please enter a number.\n");
      goto LOOP_;
    }

    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "Select permission option: %d", option);
    wrefresh(path_win);  // Refresh the path_win to display the selected option

    switch (option) {
      case 1:
        permissions = S_IRUSR | S_IRGRP | S_IROTH;
        break;
      case 2:
        permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        break;
      case 3:
        permissions = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP |
                      S_IXGRP | S_IROTH | S_IXOTH;
        break;
      default:
        wprintw(path_win, "Invalid option.\n");
        goto LOOP_;
        break;
    }

    if (chmod(temp_address, permissions) == 0) {
      wprintw(path_win, "Permissions changed successfully.\n");
    } else {
      wprintw(path_win, "Failed to change permissions.\n");
    }
  }
}

char *permissions(mode_t mode) {
  static char perms[10];

  perms[0] = (mode & S_IRUSR) ? 'r' : '-';
  perms[1] = (mode & S_IWUSR) ? 'w' : '-';
  perms[2] = (mode & S_IXUSR) ? 'x' : '-';
  perms[3] = (mode & S_IRGRP) ? 'r' : '-';
  perms[4] = (mode & S_IWGRP) ? 'w' : '-';
  perms[5] = (mode & S_IXGRP) ? 'x' : '-';
  perms[6] = (mode & S_IROTH) ? 'r' : '-';
  perms[7] = (mode & S_IWOTH) ? 'w' : '-';
  perms[8] = (mode & S_IXOTH) ? 'x' : '-';
  perms[9] = '\0';

  return perms;
}

void show_file_info(char *files[]) {
  wmove(info_win, 1, 1);
  char temp_address[1000];
  total_files = 0;
  if (strcmp(files[selection], "..") != 0) {
    snprintf(temp_address, sizeof(temp_address), "%s%s",
             current_directory_->cwd, files[selection]);
    stat(temp_address, &file_stats);
    // if (isDir(file_stats.st_mode)) {
    //   get_recursive_size_directory(temp_address);
    // }

    wprintw(info_win, "Name: %s\n Type: %s\n Size: %.2f KB\n Permissions: %s\n",
            files[selection], isDir(file_stats.st_mode) ? "Folder" : "File",
            isDir(file_stats.st_mode)
                ? get_recursive_size_directory(temp_address)
                : (float)file_stats.st_size / (float)1024,
            permissions(file_stats.st_mode));
    wprintw(info_win, " No. Files: %zu\n",
            isDir(file_stats.st_mode) ? total_files : 1);
    wprintw(info_win, "\n\n Shortcuts:\n");
    wprintw(info_win, "  - Arrow Up/Down: Navigate through files\n");
    wprintw(info_win, "  - Enter: Open selected file or directory\n");
    wprintw(info_win, "  - n: Make a new file\n");
    wprintw(info_win, "  - N: Make a new folder\n");
    wprintw(info_win, "  - r: Rename a file\n");
    wprintw(info_win, "  - c: Copy a file\n");
    wprintw(info_win, "  - m: Move a file\n");
    wprintw(info_win, "  - p: Change permissions\n");
    wprintw(info_win, "  - d: Delete a file\n");
    wprintw(info_win, "  - h: Show this help screen\n");
    wprintw(info_win, "  - q: Quit the program\n");
  } else {
    wprintw(info_win, "Press Enter to go back\n");
    wprintw(info_win, "\n\n Shortcuts:\n");
    wprintw(info_win, "  - Arrow Up/Down: Navigate through files\n");
    wprintw(info_win, "  - Enter: Open selected file or directory\n");
    wprintw(info_win, "  - n: Make a new file\n");
    wprintw(info_win, "  - N: Make a new folder\n");
    wprintw(info_win, "  - r: Rename a file\n");
    wprintw(info_win, "  - c: Copy a file\n");
    wprintw(info_win, "  - m: Move a file\n");
    wprintw(info_win, "  - p: Change permissions\n");
    wprintw(info_win, "  - d: Delete a file\n");
    wprintw(info_win, "  - h: Show this help screen\n");
    wprintw(info_win, "  - q: Quit the program\n");
  }
}

void make_folder() {
  char folder_name[100];
  wprintw(path_win, "Enter the name of the folder: ");
  wgetstr(path_win, folder_name);

  char folder_path[1000];
  snprintf(folder_path, sizeof(folder_path), "%s%s", current_directory_->cwd,
           folder_name);
  if (mkdir(folder_path, 0777) == 0) {
    wprintw(path_win, "Folder created successfully.\n");
  } else {
    wprintw(path_win, "Failed to create folder.\n");
  }
}

void make_file() {
  char file_name[100];
  wprintw(path_win, "Enter the name of the file: ");
  wgetstr(path_win, file_name);

  char file_path[1000];
  snprintf(file_path, sizeof(file_path), "%s%s", current_directory_->cwd,
           file_name);

  int mode = 0;
  char permission_choice[10];
  wprintw(path_win, "Choose file permissions:\n");
  wprintw(path_win, "1. Read only\n");
  wprintw(path_win, "2. Read and write\n");
  wprintw(path_win, "3. Read, write, and execute\n");
  wgetstr(path_win, permission_choice);

  switch (atoi(permission_choice)) {
    case 1:
      mode = S_IRUSR;
      break;
    case 2:
      mode = S_IRUSR | S_IWUSR;
      break;
    case 3:
      mode = S_IRUSR | S_IWUSR | S_IXUSR;
      break;
    default:
      wprintw(
          path_win,
          "Invalid choice. File will be created with default permissions.\n");
      break;
  }

  FILE *file = fopen(file_path, "w");
  if (file != NULL) {
    chmod(file_path, mode);
    wprintw(path_win, "File created successfully.\n");
    fclose(file);
  } else {
    wprintw(path_win, "Failed to create file.\n");
  }
}

int main() {
  int i = 0;
  init();
  init_curses();
  getcwd(current_directory_->cwd, sizeof(current_directory_->cwd));
  strcat(current_directory_->cwd, "/");
  current_directory_->parent_dir =
      strdup(get_parent_directory(current_directory_->cwd));
  int ch;
  // init_windows();
  do {
    // int temp_len;

    len = get_no_files_in_directory(current_directory_->cwd);

    len = len <= 0 ? 1 : len;

    // char *files[temp_len], *temp_dir;
    char *files[len], *temp_dir;
    get_files(current_directory_->cwd, files);
    // qsort(files, len, sizeof(char *), compare);
    // sort(files, len);
    if (selection > len - 1) {
      selection = len - 1;
    }

    getmaxyx(stdscr, maxy, maxx);
    maxy -= 2;
    int t = 0;
    init_windows();

    for (i = start; i < len; i++) {
      if (t == maxy - 1)
        break;
      int size = snprintf(NULL, 0, "%s%s", current_directory_->cwd, files[i]);
      // printf("%d", size);
      if (i == selection) {
        // mvprintw(i + 2, 0, "Hm");
        wattron(current_win, A_STANDOUT);
      } else {
        wattroff(current_win, A_STANDOUT);
      }

      temp_dir = malloc(size + 1);
      snprintf(temp_dir, size + 1, "%s%s", current_directory_->cwd, files[i]);

      stat(temp_dir, &file_stats);
      isDir(file_stats.st_mode) ? wattron(current_win, COLOR_PAIR(1))
                                : wattroff(current_win, COLOR_PAIR(1));
      wmove(current_win, t + 1, 2);
      wprintw(current_win, "%.*s\n", maxx, files[i]);
      // mvprintw(i + 2, 2, "%s", temp_dir);
      free(temp_dir);
      t++;
    }
    wmove(path_win, 1, 0);
    wprintw(path_win, " %s", current_directory_->cwd);
    show_file_info(files);
    refreshWindows();

    switch ((ch = wgetch(current_win))) {
      case KEY_UP:
      case KEY_NAVUP:
        // mvprintw(i + 2, 2, "lol");
        scroll_up();
        break;
      case KEY_DOWN:
      case KEY_NAVDOWN:
        scroll_down();
        break;
      case KEY_ENTER:
        handle_enter(files);
        break;
      case 'r':
      case 'R':
        rename_file(files);
        break;
      case 'c':
      case 'C':
        copy_files(files);
        break;
      case 'n':
        make_file();
        break;
      case 'N':
        make_folder();
        break;
      case 'm':
      case 'M':
        move_file(files);
        break;
      case 'p':
      case 'P':
        change_permissions(files);
        break;
      case 'd':
      case 'D':
        delete_file(files);
        break;
    }
    for (i = 0; i < len; i++) {
      free(files[i]);
    }
    // free(files);
  } while (ch != 'q');
  endwin();
}