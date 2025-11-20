# MechaCrypt: Hardware-Encrypted Messaging System

This repository contains the source code and documentation for MechaCrypt, with both **FPGA (UPduino v3.1 )** and **MCU (Nucleo-L432KC)**.  

**Project Report Website:** [https://rex-josaphat.github.io/MechaCrypt-Portfolio/pages/](https://rex-josaphat.github.io/MechaCrypt-Portfolio/pages/)

## Repository Structure

- `sender/`: Contains the FPGA and MCU code for the sender side of the MechaCrypt system.

  - `fpga/`: Contains SystemVerilog code and testench implementing AES encryption, SPRAM read/write and bit-banged byte sending over mechanical actuators.
  - `mcu/`: Contains C code implementing Web server for message input and parsing in ASCII, TRNG for encryption key generation, SPI communication with FPGA to send keys, plaintext and cyphertext.

- `receiver/`: Contains the FPGA and MCU code for the receiver side of the MechaCrypt system.
  - `fpga/`: Contains SystemVerilog code and testbench implementing AES decryption and bridge code to .
  - `mcu/`: Contains C code for reconstructing full messages from mechanical actuator inputs, displaying progress on a 16×4 LCD, parsing received plaintext, and displaying on web server.

## Tech Stack

| Domain    | Tools / Technologies                        |
| --------- | ------------------------------------------- |
| Hardware  | Lattice UP5K FPGA, STM32L432KC MCU, ESP8266 |
| Languages | SystemVerilog, Embedded C, HTML, CSS        |
| Protocols | I2C, SPI, USART, HTTP                       |
| UI        | Web dashboard/server + 16×4 LCD             |

## Authors

- **Josaphat Ngoga**

  - **Github:** [https://github.com/Rex-Josaphat](https://github.com/Rex-Josaphat)
  - **Website:** [https://jngoga.vercel.app/](https://jngoga.vercel.app/)
  - **E155 Portfolio:** [https://rex-josaphat.github.io/E155-Portfolio/](https://rex-josaphat.github.io/E155-Portfolio/)

- **Christian Wu**
  - **Github:** [https://github.com/chrwu17](https://github.com/chrwu17)
  - **E155 Portfolio:** [https://chrwu17.github.io/hmc-e155-portfolio/](https://chrwu17.github.io/hmc-e155-portfolio/)
