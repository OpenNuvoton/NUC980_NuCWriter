The example_f folder shows a reference solution of firmware update.

In the production stage
--------------------------
Use "run_spinor_f.ini" to program SPI NOR flash. It program the u-boot, env, and Linux kernel to SPI NOR.
It also program the "NuWriterFW_64MB.bin", which will be used in firmware update process.


Do firmware update without change power setting
---------------------------------------------------------
1. In Linux, overwrite the u-boot env with "load_NuWriterFW_env.txt" and reset NUC980.
2. Then NUC980 will reset and run u-boot. u-boot will load and run NuWriterFW_64MB.bin. After NuWriterFW_64MB.bin
   run, NUC980 should be able to connect the NuCWriter in firmware update mode.
3. On Windows PC, use NuCWriter to updtae kernel and env with "firmware_update = yes" to enable NuCWriter firmware update mode.
   Example: run_spinor_firmware_update.ini  
4. After updtae complete, NuCWriter will issue a USB reset to reset NUC980.
   Then, NUC980 will boot with the new firmware.


