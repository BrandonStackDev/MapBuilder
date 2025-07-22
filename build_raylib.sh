#!/bin/bash

# cmake .. \
#   -DPLATFORM="Desktop" \
#   -DGRAPHICS="GRAPHICS_API_OPENGL_30" \
#   -DBUILD_SHARED_LIBS="ON" \
#   -DCMAKE_BUILD_TYPE="Release" \
#   -DBUILD_EXAMPLES="OFF"
# make -j$(nproc)
# sudo make install
# sudo ldconfig

# #note, this does not work
# cmake .. -DPLATFORM="Desktop" -DGRAPHICS="GRAPHICS_API_OPENGL_ES_30"
# make -j$(nproc)
# sudo make install
# sudo ldconfig

cmake .. -DPLATFORM="Desktop" -DGRAPHICS="GRAPHICS_API_OPENGL_21"
make
sudo make install
sudo ldconfig

