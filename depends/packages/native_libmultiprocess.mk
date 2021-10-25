package=native_libmultiprocess
$(package)_version=34ce921d2daecb0b20772363621554a2b763b1db
$(package)_download_path=https://github.com/chaincodelabs/libmultiprocess/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=d0b85d3eb3fe9370c5feda2f742f5f3aa9cc83f989f4477452f60e332d64d71a
$(package)_dependencies=native_capnp

define $(package)_config_cmds
  $($(package)_cmake)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
