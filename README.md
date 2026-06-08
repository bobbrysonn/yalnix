# Yalnix

This repository contains a Yalnix kernel implementation and user-level test
programs for the COSC 58 Yalnix project.

## Layout

- `src/`: kernel-level source and headers.
- `user/`: user-level programs and regression tests.
- `docs/`: checkpoint test notes and supporting LaTeX documents.
- `Makefile`: builds the kernel executable and user programs.

## Build

On Thayer, set the framework path before building:

```sh
export YALNIX_FRAMEWORK=/thayerfs/courses/26spring/cosc058/workspace/yalnix_framework
make clean
make
```

The build leaves user executables at the repository root so they can be passed
directly to `yalnix`, for example:

```sh
./yalnix -W init
./yalnix -W final_ipc
```

## Tests

Useful local regression tests include:

```sh
./yalnix -W cp4_fork
./yalnix -W cp4_exec
./yalnix -W cp4_stack
./yalnix -W cp5_tty
./yalnix -W final_ipc
```

Terminal input tests require the Yalnix terminal support. In an SSH session
without a display, run them through `xvfb-run`:

```sh
printf "abcdef\nxyz\n" > cp5_input.txt
printf "\n" | xvfb-run -a timeout 12 ./yalnix -W -x -I0 cp5_input.txt cp5_read

printf "hello\n" > tty_rw_input.txt
printf "\n" | xvfb-run -a timeout 12 ./yalnix -W -x -I0 tty_rw_input.txt cp5_tty_rw
```

The framework tests in `yalnix_framework/sample/test` are also part of the
final regression set. The `torture` test is intentionally non-terminating, so a
timeout is expected when it runs successfully for a fixed window without a
`-W` abort.

## Final Scope

The implementation covers the required undergraduate final submission scope:
process syscalls, virtual memory growth, terminal I/O, pipes, locks, condition
variables, reclaim, and the required traps/exceptions. Optional Section 9 extra
functionality is not implemented.
