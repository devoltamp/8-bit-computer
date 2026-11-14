> [!CAUTION]
> output register does not work properly, main problem is that `I/O` pins are pulled up high. so, the output stays high no matter what. except that everything works just fine.

![click image to view clearly](images/main.SVG)
> [!NOTE]
> open the image in a new tab, for better view

## test #0001
custom assembly instructions, name it whatever you want.
```assembly
LDA 14
ADD 15
OUT
HLT
```
breakdown,
|counter|inst. set|value|
|---|---|---|
0000|0001|1110 - 00011100 (28)
0001|0010|1111 - 00001110 (14)

### `LDA 14`
*loads the value from the mem. via add. into the reg. `A`*  
CO  MI  
RO  II  
CE  
IO  MI  
RO  AI

### `ADD 15`
*loads the value from the mwm. via add. into the reg. `B`*  
CO  MI  
RO  II  
CE  
IO  MI  
RO  BI  
EO  AI

### `OUT`
*simply on the o/p reg.*    
CO  MI  
RO  II  
CE  
AO  OI

### `HLT`
*to simply holt the clock*  
HLT

### `summary`
|time|LDA 14|ADD 15|OUT|
|---|---|---|---|
|TO|CO  MI|CO  MI|CO  MI|
|TI|RO  II|RO  II|RO  II|
||CE|CE|CE|
|T2|IO  MI|IO  MI|AO  OI|
|T3|RO  AI|RO  BI||
|T4||EO  AI||  

*or*
| Mnemonic | INSTRUCTION | STEP | HLT | MI | RI | RO | IO | II | AI | AO | ΣO | SU | BI | OI | CE | CO | J |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Fetch** | `XXXX` | `000` | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 |
| | `XXXX` | `001` | 0 | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 |
| **LDA** | `0001` | `010` | 0 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| | `0001` | `011` | 0 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| | `0001` | `100` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| **ADD** | `0010` | `010` | 0 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| | `0010` | `011` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| | `0010` | `100` | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| **OUT** | `1110` | `010` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| | `1110` | `011` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| | `1110` | `100` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| **HLT** | `1111` | `010` | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| | `1111` | `011` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| | `1111` | `100` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |  

- 8bits as i/p aka address & 16bits of o/p aka contents, that are stored in the eeprom x2.
