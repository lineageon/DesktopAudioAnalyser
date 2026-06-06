# Desktop Audio Analyzer (C++ / JUCE)

A real-time, low-latency desktop audio analysis tool built with C++ and the JUCE framework. Designed to interface directly with ASIO hardware (e.g., Focusrite Scarlett) to provide zero-latency visual feedback for live microphone and instrument inputs.

## Features
* **Real-Time DSP:** Processes incoming audio buffers directly on the high-priority audio thread.
* **FFT Spectrum Analyzer:** Utilizes a 1024-point Fast Fourier Transform (with a Hann window) to visualize frequency response.
* **Live Waveform Generation:** Captures and scales raw input signals into a scrolling GUI element.
* **Ballistic Peak Metering:** Tracks transient peaks with a smoothed mathematical decay for realistic studio-metering behavior.
* **Hardware Integration:** Programmatically hunts and locks onto available ASIO drivers, automatically summing stereo inputs into a centered mono mix.

## Tech Stack
* **Language:** C++17
* **Framework:** JUCE 7/8
* **Concepts Demonstrated:** Digital Signal Processing (DSP), Audio Buffer Management, FIFO Queues, GUI/Audio Thread Separation, Memory Management.

## How to Build
1. Clone the repository.
2. Open `DesktopAudioAnalyser.jucer` in the **Projucer**.
3. Ensure you have the `juce_dsp` and `juce_audio_devices` modules enabled (with ASIO support if on Windows).
4. Click "Save and Open in IDE" to generate the Visual Studio or Xcode project.
5. Compile and run.

## Demo
<img width="1346" height="1076" alt="Screenshot 2026-06-06 180342" src="https://github.com/user-attachments/assets/6f0a2fde-71b2-419c-9c17-f44869da5ce2" />

<img width="1351" height="1061" alt="Screenshot 2026-06-06 180233" src="https://github.com/user-attachments/assets/60ccedd2-6250-4505-a65c-5e6e02d2b641" />
