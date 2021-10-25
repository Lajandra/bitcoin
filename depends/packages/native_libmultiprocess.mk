package=native_libmultiprocess
<<<<<<< HEAD
$(package)_version=1af83d15239ccfa7e47b8764029320953dd7fdf1
||||||| parent of 679bfa51770 (Update libmultiprocess library)
$(package)_version=d576d975debdc9090bd2582f83f49c76c0061698
=======
$(package)_version=7d10f3b1e39caa04b3fcddebf720fd3a2b54c21d
>>>>>>> 679bfa51770 (Update libmultiprocess library)
$(package)_download_path=https://github.com/chaincodelabs/libmultiprocess/archive
$(package)_file_name=$($(package)_version).tar.gz
<<<<<<< HEAD
$(package)_sha256_hash=e5587d3feedc7f8473f178a89b94163a11076629825d664964799bbbd5844da5
||||||| parent of 679bfa51770 (Update libmultiprocess library)
$(package)_sha256_hash=9f8b055c8bba755dc32fe799b67c20b91e7b13e67cadafbc54c0f1def057a370
=======
$(package)_sha256_hash=1e669deea827756c396e9740cac6581c3c8758e206320d359c520fc76ff31f53
>>>>>>> 679bfa51770 (Update libmultiprocess library)
$(package)_dependencies=native_capnp

define $(package)_config_cmds
  $($(package)_cmake) .
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-bin
endef
