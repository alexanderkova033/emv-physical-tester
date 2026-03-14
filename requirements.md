## EMV Physical Card Inserter – Given Requirements

### 1. Scenario

- Main scenario:
  - Insertion and removal of a card into/from an EMV contact chip card reader on command from JUnit or other popular JVM testing tools.

### 2. Terminals

- Form factor: normal attended payment terminal (not unattended).

### 3. Interface

- The customer wants the interface to be “more standard and high-level”.
- The interface must allow any tester to easily read or change the request and the response.

### 4. Operation Rate

- Expected operating rate: about 1–2 insertion/removal cycles per minute.

### 5. Mechanical Device Requirements

- It is required to design the physical device that will insert and remove the card, not only to write the program.
- Parts for this device must be purchasable worldwide.
- The device must be very easy to assemble.
- The device’s parts must be sufficiently standard so that:
  - It is easy to find and buy them.
  - It is easy to assemble the device.
  - It is easy to reconfigure the device for different terminal models.
  - It is easy to debug mechanical and software issues.

### 6. Programming Language

- The programming language for the software must be Kotlin.

