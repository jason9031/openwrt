include $(TOPDIR)/rules.mk

PKG_NAME:=mt76x8-pwm
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=LICENSE

PKG_MAINTAINER:=Edward <oxp@yystart.com>
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

define KernelPackage/mt76x8-gpio-pwm
	SUBMENU:=Other modules
	TITLE:=MT76X8 GPIO PWM Driver
	DEPENDS:=@TARGET_ramips 
	FILES:=$(PKG_BUILD_DIR)/mt76x8-gpio-pwm.ko
	AUTOLOAD:=$(call AutoLoad,91,mt76x8-gpio-pwm,1)
endef

EXTRA_KCONFIG:= \
	CONFIG_MT76X8_GPIO_PWM=m

EXTRA_CFLAGS:= \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG)))) \

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	SUBDIRS="$(PKG_BUILD_DIR)" \
	EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
	$(EXTRA_KCONFIG)

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,mt76x8-gpio-pwm))
