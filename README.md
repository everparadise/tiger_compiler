# Tiger Compiler Labs in C++

## Contents

- [Tiger Compiler Labs in C++](#tiger-compiler-labs-in-c)
  - [Contents](#contents)
  - [Overview](#overview)
  - [Difference Between C Labs and C++ Labs](#difference-between-c-labs-and-c-labs)
  - [Installing Dependencies](#installing-dependencies)
  - [Compiling and Debugging](#compiling-and-debugging)
  - [Other Commands](#other-commands)
  - [External Documentations](#external-documentations)

## Overview

We rewrote the Tiger Compiler labs using the C++ programming language because some features in C++ like inheritance and polymorphism
are more suitable for these labs and less error-prone.

We provide you all the codes of all labs at one time. In each lab, you only
need to code in some of the directories.

## Difference Between C Labs and C++ Labs

1. Tiger compiler in C++ uses [flexc++](https://fbb-git.gitlab.io/flexcpp/manual/flexc++.html) and [bisonc++](https://fbb-git.gitlab.io/bisoncpp/manual/bisonc++.html) instead of flex and bison because flexc++ and bisonc++ is more flexc++ and bisonc++ are able to generate pure C++ codes instead of C codes wrapped in C++ files.

2. Tiger compiler in C++ uses namespace for modularization and uses inheritance and polymorphism to replace unions used in the old labs.

3. Tiger compiler in C++ uses CMake instead of Makefile to compile and build the target.

<!---4. We've introduced lots of modern C++-style codes into tiger compiler, e.g., smart pointers, RAII, RTTI. To get familiar with the features of modern C++ and get recommendations for writing code in modern C++ style, please refer to [this doc](https://ipads.se.sjtu.edu.cn/courses/compilers/tiger-compiler-cpp-style.html) on our course website.-->

## Installing Dependencies

We provide you a Docker image that has already installed all the dependencies. You can compile your codes directly in this Docker image. Visit [https://ipads.se.sjtu.edu.cn/courses/compilers/tiger-compiler-environment.html](https://ipads.se.sjtu.edu.cn/courses/compilers/tiger-compiler-environment.html) to find a detailed guide.

## Compiling and Debugging

There are five makeable targets in total, including `test_slp`, `test_lex`, `test_parse`, `test_semant`,  and `tiger-compiler`.

1. Run container environment and attach to it, here is a simple example:

```bash
# Run container and directly attach to it
docker run -it --privileged -p 2222:22 \
    -v $(pwd):/home/stu/tiger-compiler ipadsse302/tigerlabs_env:latest  # or `make docker-run`
# Or run container in the backend and attach to it later
docker run -dt --privileged -p 2222:22 \
    -v $(pwd):/home/stu/tiger-compiler ipadsse302/tigerlabs_env:latest
docker attach ${YOUR_CONTAINER_ID}
```

You can also follow the guide on [https://ipads.se.sjtu.edu.cn/courses/compilers/tiger-compiler-environment.html](https://ipads.se.sjtu.edu.cn/courses/compilers/tiger-compiler-environment.html) to build a container. It offers a user-friendly script for Windows users and addresses most networking issues related to Docker Hub.

2. Build in the container environment

```bash
mkdir build && cd build && cmake .. && make test_xxx  # or `make build`
```

3. Debug using gdb or any IDEs

```bash
gdb test_xxx # e.g. `gdb test_slp`
```

**Note: we will use `-DCMAKE_BUILD_TYPE=Release` to grade your labs, so make
sure your lab passed the released version**

## Other Commands

Utility commands can be found in the `Makefile`. They can be directly run by `make xxx` in a Unix shell. Windows users cannot use the `make` command, but the contents of `Makefile` can still be used as a reference for the available commands.

## External Documentations

You can read external documentations on our course website:

- [Lab Assignments](https://ipads.se.sjtu.edu.cn/courses/compilers/labs.shtml)
- [Environment Configuration of Tiger Compiler Labs](https://ipads.se.sjtu.edu.cn/courses/compilers/tiger-compiler-environment.html)
<!---- [Tiger Compiler in Modern C++ Style](https://ipads.se.sjtu.edu.cn/courses/compilers/tiger-compiler-cpp-style.html)-->

