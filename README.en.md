# MIPI Code Example Project

This is a C language project designed to demonstrate MIPI camera data acquisition and JPEG encoding. The project primarily implements camera initialization, YUV data acquisition, and encoding YUV data into JPEG format using the MPP (Media Process Platform) library.

## Main Features

- Camera initialization and configuration
- Starting video capture
- Capturing video frames in YUV format
- Feeding YUV data into the MPP encoder buffer
- Encoding video frames into JPEG format and saving them to a file

## File Structure

- `inc/camera_init.h` - Function declarations for camera initialization and operations
- `lib/camera_init.c` - Function definitions for camera initialization and operations
- `src/mipi_main.c` - Main program entry point
- `src/mipi_main_back.c` - Backup implementation containing the main program and MPP encoding-related functions

## Usage

1. Ensure your device supports the MIPI camera interface and the camera module is properly connected.
2. Compile the project (requires the MPP library and related development tools to be installed).
3. Run the generated executable file; the program will automatically capture one YUV frame and encode it into a JPEG file.

## Dependencies

- MPP (Media Process Platform) library
- C compiler (e.g., GCC)
- CMake (for building)

## Notes

- Ensure the camera device path, resolution, and pixel format match the actual hardware parameters.
- The output JPEG file name and path can be modified in the main function.

## License

This project is licensed under the MIT License. See the LICENSE file in the repository for details.