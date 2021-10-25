package=native_libmultiprocess
$(package)_version=bc6624a5d3884375eaf6ba5cfba91f096825b5ca
$(package)_download_path=https://github.com/chaincodelabs/libmultiprocess/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=47ec51b229dbe8581efa79a8a5a4994f88fbafc5f34f8022836920d5ad69b224
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
