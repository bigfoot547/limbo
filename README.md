# limbo
A (hopefully) high-performance Minecraft limbo server

## Project notes
- __Currently incomplete, just wanted this here to track development for now__
- Requires linux (relies on epoll currently, feel free to contribute other socket engines like poll or kqueue or /dev/poll)
- Requires linux kernel version >2.6.2 when using epoll
- Requires some kind of pthread implementation
- Sort of a hello world project in C, I've not made any other large projects from scratch in C like this
- Trying to support a wide range of Minecraft client versions (bare minimum 1.7.10-latest)
