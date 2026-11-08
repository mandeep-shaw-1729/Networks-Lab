# Networks-Lab
##Assignment 3
To compile and execute your 3-tier CSMA/CD network simulation suite, follow these steps. Since the architecture involves a **Server**, a **Receiver**, and one or more **Sender Stations**, you will need to run them in separate terminal windows (or tabs).

Open your terminal in the source code directory and compile each component using `gcc`:

* **Compile the Server:**
```bash
gcc server.c fileio.c frame.c -o server
```
for Windows,
```bash
gcc server.c fileio.c frame.c -o server.exe -lws2_32
```
* **Compile the Receiver:**
```bash
gcc receiver.c fileio.c frame.c -o receiver
```
for Windows,
```bash
gcc receiver.c fileio.c frame.c -o receiver.exe -lws2_32
```
* **Compile the Sender:**
```bash
gcc sender.c fileio.c frame.c -o sender
```
for Windows,
```bash
gcc sender.c fileio.c frame.c -o sender.exe -lws2_32
```
* **Compile the Server, Receiver and Sender to:**
```bash
gcc server.c fileio.c frame.c -o server && gcc receiver.c fileio.c frame.c -o receiver && gcc sender.c fileio.c frame.c -o sender
```
for Windows,
```bash
gcc server.c fileio.c frame.c -o server.exe -lws2_32 && gcc receiver.c fileio.c frame.c -o receiver.exe -lws2_32 && gcc sender.c fileio.c frame.c -o sender.exe -lws2_32
```
### Step 3: Execute the Components (In Order)

Open **three separate terminal windows** and run the programs in the following strict order:

#### 1. Start the Server First

Run the server and specify the expected number of sender stations (e.g., `2`):

```bash
./server 2
```
or
```bash
./server
```
for Windows,
```bash
server.exe 2
```
or
```bash
server.exe
```
#### 2. Start the Receiver Second

Run the receiver node to listen for delivered frames from the server:

```bash
./receiver
```
for Windows,
```bash
receiver.exe
```
*(Choose `1` for verbose frame details if prompted)*.

#### 3. Start the Sender Station(s) Last

In separate terminal windows, start your sender stations with unique names:
* **Sender 1:**
```bash
./sender Station_A
```
* **Sender 2 (in another terminal):**
```bash
./sender Station_B
```
or you can simply do,
```bash
./sender
```
Then, name them individually.

---
for Windows,
```bash
sender.exe Station_A
```
* **Sender 2 (in another terminal):**
```bash
sender.exe Station_B
```
or you can simply do,
```bash
sender.exe
```
Then, name them individually.

When the first sender connects, the server will prompt it to choose or lock the CSMA protocol (Non-Persistent, 1-Persistent, p-Persistent, or CSMA/CD) for the session. Once all senders finish transmitting their file payloads, the server and receiver will automatically print out summary statistics (throughput, collision counts, and delays).
