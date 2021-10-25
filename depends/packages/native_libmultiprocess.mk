package=native_libmultiprocess
$(package)_version=306c8b100d20eb8a2865b912730851b7d28a11a8
$(package)_download_path=https://github.com/chaincodelabs/libmultiprocess/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=c45e6c6ad683af2ae07f1e01037cd3037b41b3a1da90ab1c240cfced3cb1cfff
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
