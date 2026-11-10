## IMPROVEMENTS

- extract process information with ebpf backend
  - date
  - file
  - process Name
  - event Read, write, execute, delete, move
  - state of the process
  - umask
  - username: user name of the process
  - command line arguments
  - success: true or false
- Nested directories: 2-level
- Proper documentation
- Drop and maintain only necessary root CAPs

### Config format

[directories]
path ...

[files]
