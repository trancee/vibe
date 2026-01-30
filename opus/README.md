### Key Features Implemented

- **CPU (6510)**: All legal opcodes, addressing modes, decimal mode ADC/SBC, interrupt handling, CPU port ($00/$01) with proper floating state
- **Memory**: PLA bank switching logic, ROM visibility control, Color RAM, I/O mapping
- **VIC-II**: Raster timing, bad line detection, BA stall, raster IRQ, ANSI console rendering
- **CIA**: Timer A/B with pipeline delays, ICR bit 7 timing, TOD clock, keyboard matrix scanning
- **SID**: Register access, envelope generator, oscillators, filter (simplified), open bus behavior
- **System**: Central tick mechanism ensuring all components stay synchronized

### Build & Run

```sh
make           # Build the emulator
make run       # Run the emulator
make debug     # Run with debug output
make test      # Run for 100 frames (testing)
```
