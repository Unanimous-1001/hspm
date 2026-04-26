CXX      = g++

#   make PREFIX=/opt/hspm LIVE=/usr install
PREFIX    ?= /opt/hspm
LIVE      ?= /usr
DISTFILES ?= /usr/src/distfiles/

CXXFLAGS = -std=c++17 -Wall -Wextra -Isrc \
           -DHSPM_ROOT_PATH=\"$(PREFIX)\" \
           -DHSPM_LIVE_PATH=\"$(LIVE)\" \
           -DHSPM_DISTFILES_PATH=\"$(DISTFILES)\"
LDFLAGS  = -lsqlite3

SRCS := $(shell find src -name '*.cpp')
OBJS := $(SRCS:.cpp=.o)

TARGET = hspm

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	find src -name '*.o' -delete
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/bin/hspm
	mkdir -p $(PREFIX)/{store,db,logs,recipes,builders,tools}
	cp recipes/*.recipe  $(PREFIX)/recipes/ 2>/dev/null || true
	cp builders/*.sh     $(PREFIX)/builders/
	cp    tools/blfs-scraper.py $(PREFIX)/tools/
	cp    src/db/schema.sql     $(PREFIX)/db/
	cp    blfs-urls.txt         $(PREFIX)/ 2>/dev/null || true
	chmod +x $(PREFIX)/builders/*.sh
	chmod +x $(PREFIX)/tools/blfs-scraper.py
	@echo "HSPM installed to $(PREFIX)"
	@echo "Run 'hspm init' to initialize the database"

TEST_FLAGS = -std=c++17 -Isrc
TEST_LIBS  = -lsqlite3

test_recipe: tests/test_recipe_parser.cpp src/core/package.cpp
	$(CXX) $(TEST_FLAGS) $^ -o test_recipe_parser
	./test_recipe_parser

test_graph: tests/test_graph.cpp src/core/graph.cpp src/core/package.cpp src/db/database.cpp
	$(CXX) $(TEST_FLAGS) $^ -o test_graph $(TEST_LIBS)
	./test_graph

test_checksum: tests/test_checksum.cpp src/fetch/checksum.cpp
	$(CXX) $(TEST_FLAGS) $^ -o test_checksum
	./test_checksum

test_collision: tests/test_collision.cpp src/store/collision.cpp src/db/database.cpp
	$(CXX) $(TEST_FLAGS) $^ -o test_collision $(TEST_LIBS)
	./test_collision

test_symlink: tests/test_symlink_transaction.cpp src/store/symlink.cpp src/store/manifest.cpp src/db/database.cpp
	$(CXX) $(TEST_FLAGS) $^ -o test_symlink $(TEST_LIBS)
	./test_symlink

tests: test_recipe test_graph test_checksum test_collision test_symlink
	@echo "\nAll test suites complete."

.PHONY: clean install tests test_recipe test_graph test_checksum test_collision test_symlink
.PHONY: clean
