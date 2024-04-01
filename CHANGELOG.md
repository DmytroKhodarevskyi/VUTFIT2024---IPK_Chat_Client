## Graceful shutdown

The server can be gracefully shut down by sending a `SIGINT` signal to the server process. The server will then close all the connections and free all the resources. However memory leaks weren't tested much, si possibly leaks can occur.

Also shutting down in some cases (waiting for a response from a server) can not work properly and the client can hang. This is a known issue and it wasn't fixed.

## Errors return

The client doesnt support proper error return codes. It just prints the error message to the `stderr` and exits with different codes. Unfortunately i didn't have enough time to properly organise it, so it is not implemented yet. This is a known issue and it wasn't fixed.

## Infinite waiting

I added timeout for client waiting for a response from a server in some cases, just to be sure that it doesn't end up in a **deadlock**. 

## Little sleeps

In some cases i added little sleeps to the client to make while spinning slower, for safeness purposes. This is not a good practice, but it was safer to test, and doesn't change the client behavior much.