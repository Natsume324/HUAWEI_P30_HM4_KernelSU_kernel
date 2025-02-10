export ARCH=arm64  
export PATH=$PATH:/root/aarch64-linux-android-4.9-android10/bin:/root/clang/clang-r353983c/bin
export CROSS_COMPILE=aarch64-linux-android-
export CLANG_PREBUILTS_PATH=/root/clang/clang-r353983c/
export LD_LIBRARY_PATH=/root/clang/clang-r353983c/lib64/

mkdir out
make ARCH=arm64 O=out CC="ccache clang" merge_kirin980_defconfig
make ARCH=arm64 O=out CC="ccache clang" -j$(nproc --all) 2>&1 | tee build.log

echo 复制Image.gz
cp out/arch/arm64/boot/Image.gz  tools/Image.gz
