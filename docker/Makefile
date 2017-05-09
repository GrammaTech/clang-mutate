DOCKER_REGISTRY_HOST ?= docker.grammatech.com:14850

DOCKER_FLAGS=

BUILD_TAG ?= latest
BUILD_FROM_TAG ?= $(BUILD_TAG)

FILEPP_OPTIONS = -D __GTD__BUILD_TAG=$(BUILD_TAG) \
                 -D __GTD__BUILD_FROM_TAG=$(BUILD_FROM_TAG)

IMAGE = clang-mutate

.PHONY: FORCE
FORCE:

$(IMAGE): % : %/Dockerfile
	echo "Making $(LOCAL_IMAGE_NAME)"
	docker build $(DOCKER_FLAGS) -f $< -t $(LOCAL_IMAGE_NAME) $@

## Preproccess Dockerfile.in to Dockerfile
$(IMAGE)/Dockerfile: $(IMAGE)/Dockerfile.in %/ci_git_repo config
	filepp -b -lc \\\\ $< -o $@ -I ./config $(FILEPP_OPTIONS)
	rm -f $@~

## Preproccess Dockerfile.in to Dockerfile
%/Dockerfile: %/Dockerfile.in config
	filepp -b -lc \\\\ $< -o $@ -I ./config $(FILEPP_OPTIONS)
	rm -f $@~

%/ci_git_repo: FORCE
	mkdir -p $@
	rsync -av --exclude='/docker' --exclude='.git' $(CI_PROJECT_DIR)/ ./$@

## Ensure that the global docker configuration is in place.
config: FORCE
	mkdir -p $@
	curl https://git.grammatech.com/gt/docker-base/raw/master/docker/config/GTDockerConfig.h > $@/GTDockerConfig.h
