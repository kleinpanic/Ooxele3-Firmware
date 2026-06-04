FORK := $(HOME)/.local/srcs/gitclones/flipperzero-firmware-wPlugins
API_CHECK := $(FORK)/personal/toolchain/api_check.sh

.PHONY: api-check build launch

api-check:
	@bash $(API_CHECK)

build: api-check
	@ufbt build

launch: api-check
	@ufbt launch
