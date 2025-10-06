# build commands to build and tag image (from root project directory):
# !!! DONT FORGET TO CHANGE TAG (VERSION) !!!
# docker login registry.gitlab.com -u dkrasutski
# <type your gitlab password>
# docker build -t registry.gitlab.com/loko-development/loko-ground/loko-esp32:55.Y -f ./.devcontainer/Dockerfile .
# docker push registry.gitlab.com/loko-development/loko-ground/loko-esp32:55.Y
# Manually build the Docker image: docker build -f .devcontainer/Dockerfile .

ARG DOCKER_TAG=release-v5.5
FROM espressif/idf:${DOCKER_TAG}

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

RUN apt-get update -y && apt-get install \
    udev \
    default-jre \
    -y

RUN apt-get update \
    && apt-get -y install --no-install-recommends software-properties-common \
    && add-apt-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-18 main"

ENV CLANG_VERSION="19"
RUN wget https://apt.llvm.org/llvm.sh && chmod +x llvm.sh && ./llvm.sh ${CLANG_VERSION} \
    && apt-get -y install clang-format-${CLANG_VERSION} clang-tidy-${CLANG_VERSION}
# Setup symlinks for clang tools
RUN ln -s /usr/bin/clang-tidy-${CLANG_VERSION} /usr/bin/clang-tidy \
    && ln -s /usr/bin/clang-format-${CLANG_VERSION} /usr/bin/clang-format

RUN echo "source /opt/esp/idf/export.sh > /dev/null 2>&1" >> ~/.bashrc

ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]

CMD ["/bin/bash", "-c"]

# Update PIP
RUN /opt/esp/python_env/idf5.5_py3.12_env/bin/pip install \
    commitizen
