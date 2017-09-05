# Compiler and standard
CC = gcc -std=c99

# Project name
PROJ = water

# Source files
SRCS = Display.c
SRCS+= Hero.c
SRCS+= Hit.c
SRCS+= Input.c
SRCS+= Line.c
SRCS+= main.c
SRCS+= Map.c
SRCS+= Point.c
SRCS+= Sdl.c
SRCS+= Sliver.c
SRCS+= Sprites.c
SRCS+= Surfaces.c
SRCS+= Textures.c
SRCS+= Torch.c
SRCS+= util.c
SRCS+= Wall.c
SRCS+= Weapon.c

# Warnings flags
CFLAGS = -Wshadow -Wall -Wpedantic -Wextra

# Debugging flags
CFLAGS+= -g

# Optimization flags
CFLAGS+= -Ofast -flto

# Architecture flags
ARCH = -march=native -m32

# Linker flags
LDFLAGS = -Llib/linux32 -Wl,-Rlib/linux32 -lm -lc -lSDL2

# Linker
$(PROJ): $(SRCS:.c=.o)
	$(CC) $(ARCH) $(CFLAGS) $(SRCS:.c=.o) $(LDFLAGS) -o $(PROJ)

# Compiler template; generates dependency targets
%.o : %.c
	$(CC) $(ARCH) $(CFLAGS) -MT $@ -MMD -MP -MF $*.td -c $<
	@mv -f $*.td $*.d

# All dependency targets
%.d: ;
-include *.d

clean:
	rm -f $(PROJ)
	rm -f $(SRCS:.c=.o)
	rm -f $(SRCS:.c=.d)
