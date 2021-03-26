### How to compile?
# 1.install depend
## Ubuntu14.04
`$ sudo apt-get update`

`$ sudo apt-get install git g++ make libncurses5-dev subversion libssl-dev gawk libxml-parser-perl unzip wget python xz-utils vim zlibc zlib1g zlib1g-dev openjdk-8-jdk build-essential ccache gettext xsltproc` 

## Macos
note: install brew and Xcode command line tools

`$brew install coreutils findutils gawk gnu-getopt gnu-tar grep wget quilt xz`

note: gnu-getopt is keg-only, so force linking it:brew ln gnu-getopt --force

# 2.update the feeds

`$ tar xvf openwrt_cc_mt76x8.tar.gz`

`$ cd openwrt_cc_mt7688`

`$ ./scripts/feeds update -a`

`$ ./scripts/feeds install -a`

# 3.config

`$ cp Board_XiaoYin_Config .config`

`$ make menuconfig`
`select the target:`

`Target System(Ralink RT288x/RT3xxx) --->`

`Subtarget(MT7688 based board) --->`

`Target Profile(Default) --->`

## note
XiaoYin32128:32MB FLASH + 128MB RAM

XiaoYin1664:16MB FLASH + 64MB RAM

XiaoYin0864:08MB FLASH + 64MB RAM

# 5.make
`$ make -j4`

# 6.image
the binary image name like this in bin/ramips/:

 openwrt-ramips-mt7688-XiaoYinxxxx-squashfs-sysupgrade.bin