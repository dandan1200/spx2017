1. Describe how your exchange works.

My exchange relies on a few major operations to function.
Firstly, my main line. The main line is simply a waiting loop which checks every 0.5 seconds for a closed trader and closes the program when all traders have closed. This relies on the poll() function and polls the file descriptors of the named pipes for poll hangup.

Secondly, my signal handler. The signal handler is called with each SIGUSR1 that the process recieves. The signal handler completes the following steps.
- Finds the file descriptor of the trader which made the signal via its PID.
- Reads from the named pipe associated with that trader.
- Checks the message for validity. Replies to the trader with the appropriate message.
- Handles the command, for buy and sell orders, the order is first matched against all orders already in the order book before adding the order to the end of the correct order book after all matches that are possible are made.
For amend orders, the order is found and removed from the book. A new order with the amended details but the same order ID is now processed as if it is a new buy/sell order as above.
For cancel orders, the order is found and removed from the book.
- Appropriate exchange outputs are made.
- All fill messages and market broadcasts are sent and the signal handler returns to the mainline.

2. Describe your design decisions for the trader and how it's fault-tolerant.

My auto trader runs on very simple logic. It waits for a signal in the mainline. If a signal is recieved, the signal handler reads from the pipe and determines if the message is either a sell message or an accept message.
If it is a sell message, a global order struct is assigned the appropriate values from the order and the signal handler returns to the mainline after flipping a order flag. In the mainline, the order flag allows the mainline to continue to send the opposite buy order. This order string is created and written to the pipe. Next, to be fault tollerant, the trader enters a loop where it sends the signal to the exchange and sleeps for 1 second until a global accepted flag is flipped which allows the loop to exit and the trader to wait for the next command.

In the signal handler, which would interrupt the sleep(1), when an accept message is read from the pipe, the accept flag is flipped.

This allows the trader to constantly signal the exchange of its order until the signal is acknowledged as being accepted by the exchange.

3. Describe your tests and how to run them.

The end to end test cases consist of in and out files.
The testing_trader.c file can be compiled and run as a regular trader.
The in and out files represent a list of messages to be sent to the exchange in the .in and the corresponding response that should be recieved from the exchange in the same line of the .out file.

The test trader reads in 1 line of the .in file and sends it to the exchange and then goes on to the next line.
The signal handler, writes the output as soon the signal is recieved from the exchange to a new out file per trader.
The bash script RunEndToEndTests.bash will compile the exchange and trader, move the files to the correct directory and compare the trader output to the test case .out file to make sure the output is correct using the diff statement.

To run the tests, simply run the bash script.

