# Important notes on the code

**You can generate an internal overview with doxygen. For this, use `make doc-doxygen` and you'll find an internal overview of all functions and symbols in `docs/internal/html`.**

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
