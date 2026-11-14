## steps.
1. create a folder - `eeprom`
2. add the header file [`vsm.hpp`](https://github.com/mavrus95/VSMMODS/blob/master/kazus.ru/TG19264A/LCD19264A_VC/VSM.HPP)
3. add main file `28c16.cpp`
4. run this - `g++ -shared -o 28c16.dll 28c16.cpp "-Wl,--out-implib,lib28c16.a"`
5. copy the generated `28c16.dll` file 
6. paste it into the following path `C:\Program Files (x86)\Labcenter Electronics\Proteus 8 Professional\DATA\MODELS`
7. now, open the `edit properties` of the component in proteus
8. add, `PRIMITIVE=DIGITAL,EEPROM28C` & `MODDLL=28c16.dll` in the text box -- this will directly link the `.dll` file to that component
9. et voilà!

## ref.
- https://youtu.be/nsbD39le91U?si=m59twhCSl5bvU5MV
- https://youtu.be/aS5qC5VVjRo?si=V2niZzxMJlkVElmh  
  -- way too much complex, for those circles

## read test.
- Keep all address switches LOW to select address `0x000`.
- Connect `/CE` to GND.
- Connect `/OE` to GND.
- Connect `/WE` to +5 V.
- Observe D0–D7 using logic probes.

## write test.
- Set `/OE` HIGH.
- Set the address switches.
- Connect eight data inputs directly to D0–D7, with the output drivers disconnected.
- Pull `/WE` LOW and then HIGH to complete the write.
- Pull `/OE` LOW while keeping `/WE` HIGH to read the stored byte.

<!--## tests.
| `/CE` | `/OE` | `/WE` |   `Operation`    |
| :---: | :---: | :---: | :--------------: |
|  LOW  |  LOW  | HIGH  |       Read       |
|  LOW  | HIGH  |  LOW  |      Write       |
|  LOW  | HIGH  | HIGH  | Outputs disabled |-->

---
> [!NOTE] 
> everytime, the simulation is started the nano would write the data onto the ROM, but that takes a lot of time, & i ain't got time. so, i hard coded the data with the respective address. so, it does not waste time loading it. creating a factory coded ROM.
---
## hard coding rom.
eeprom memory architecture - `2048 × 8 bits`
- 2048 addressable locations
- 8-bit data per address
- address range: `0x000` - `0x7FF`
- total capacity: 16,384 bits (2 KB)
