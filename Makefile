# Convenience targets for ML tooling (Bridge plugin still builds via CMake).
PYTHON3 ?= python3
MODELS_DIR ?= ml/models
VERSION ?= v1

.PHONY: ml-register ml-bundle
ml-register:
	@test -n "$(VERSION)" || (echo "Usage: make ml-register VERSION=v1" && exit 1)
	$(PYTHON3) ml/training/model_registry.py register $(VERSION) --source $(MODELS_DIR) --root $(MODELS_DIR)

ml-bundle:
	@test -n "$(VERSION)" || (echo "Usage: make ml-bundle VERSION=v1" && exit 1)
	@mkdir -p dist
	$(PYTHON3) ml/training/bundle.py create --models-dir $(MODELS_DIR) --version $(VERSION) --out dist/$(VERSION).bridgemodels
