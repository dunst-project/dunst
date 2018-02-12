# Important notes on the code

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
