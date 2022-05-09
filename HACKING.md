# Important notes on the code

**You can generate an internal overview with doxygen. For this, use `make doc-doxygen` and you'll find an internal overview of all functions and symbols in `docs/internal/html`. You will also need `graphviz` for this.**


For people wanting to develop new features or fix bugs for dunst, here are the
steps you should take.

# Running dunst

For building dunst, you should take a look at the README. After dunst is build,
you can run it with:

        ./dunst

This might not work, however, since dunst will abort when another instance of
dunst or another notification daemon is running. You will see a message like:

        CRITICAL: [dbus_cb_name_lost:1044] Cannot acquire 'org.freedesktop.Notifications': Name is acquired by 'dunst' with PID '20937'.

So it's best to kill any running instance of dunst before trying to run the
version you just built. You can do that by making a shell function as follows
and put it in your bashrc/zshrc/config.fish:

```sh
run_dunst() {
        if make -j dunst; then
                pkill dunst
        else
                return 1
        fi
        ./dunst $@
}
```

If you run this function is the root directory of the repository, it will build
dunst, kill any running instances and run your freshly built version of dunst.

# Testing dunst

To test dunst, it's good to know the following commands. This way you can test
dunst on your local system and you don't have to wait for CI to finish.

## Run test suite

This will build dunst if there were any changes and run the test suite. You will
need `awk` for this to work (to color the output of the tests).

        make test

## Run memory leak tests

This will build dunst if there were any changes and run the test suite with
valgrind to make sure there aren't any memory leaks. You will have to build your
tests so that they free all allocated memory after you are done, otherwise this
test will fail. You will need to have `valgrind` installed for this.

        make test-valgrind


## Build the doxygen documentation

The internal documentation can be built with (`doxygen` and `graphviz` required):

        make doc-doxygen

To open them in your browser you can run something like:

        firefox docs/internal/html/index.html


# Running the tests with docker

Dunst has a few docker images for running tests on different distributions. The
documentation for this can be found at https://github.com/dunst-project/docker-images

# Comments

- Comment system is held similar to JavaDoc
    - Use `@param` to describe all input parameters
    - Use `@return` to describe the output value
    - Use `@retval` to describe special return values (like `NULL`)
    - Documentation comments should start with a double star (`/**`)
    - Append `()` to function names and prepend variables with `#` to properly reference them in the docs
- Add comments to all functions and methods
- Markdown inside the comments is allowed and also desired
- Add the comments to the prototype. Doxygen will merge the protoype and implementation documentation anyways.
  Except for **static** methods, add the documentation header to the implementation and *not to the prototype*.
- Member documentation should happen with `/**<` and should span to the right side of the member
- Test files that have the same name as a file in src/\* can include the
  associated .c file. This is because they are being compiled INSTEAD of the src
  file.


## Log messages

### Messages

- Keep your message in common format: `<problem>: <problematic value/description>`
- If you have to write text, single quote values in your sentence.

### Levels

For logging, there are printf-like macros `LOG_(E|C|W|M|I|D)`.

- `LOG_E` (ERROR):
    - All messages, which lead to immediate abort and are caused by a programming error. The program needs patching and the error is not user recoverable.
    - e.g.: Switching over an enum, `LOG_E` would go into the default case.
- `LOG_C` (CRITICAL):
    - The program cannot continue to work. It is used in the wrong manner or some outer conditions are not met.
    - e.g.: `-config` parameter value is unreadable file
- `DIE` (CRITICAL):
    - A shorthand for `LOG_C` and terminating the program after. This does not dump the core (unlike `LOG_E`).
- `LOG_W` (WARNING):
    - Something is not in shape, but it's recoverable.
    - e.g.: A value is not parsable in the config file, which will default.
- `LOG_M` (MESSAGE):
    - Important info, which informs about the state.
    - e.g.: An empty notification does get removed immediately.
- `LOG_I` (INFO):
    - Mostly unneccessary info, but important to debug (as the user) some use cases.
    - e.g.: print the notification contents after arriving
- `LOG_D` (DEBUG):
    - Only important during development or tracing some bugs (as the developer).
