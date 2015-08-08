CXXFLAGS := -std=c++1y
PREFIX := /usr

-include config.mk

TESTS := $(patsubst %.cpp,%,$(wildcard test/*.cpp))

# Include all the existing dependency files for automatic #include dependency
# handling.
-include $(TESTS:=.d)

# Build .o files and the corresponding .d (dependency) files. For more info, see
# <http://scottmcpeak.com/autodepend/autodepend.html>.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -Iinclude -MMD -MF $*.d -c $< -o $@
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $*.d

$(TESTS): %: %.o
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -lmettle -o $@

tests: $(TESTS)

.PHONY: install
install:
	cp -R include $(PREFIX)

.PHONY: test
test: tests
	mettle --color --output=verbose $(TESTS)

.PHONY: clean
clean:
	rm -f $(TESTS) test/*.o test/*.d

.PHONY: gitignore
gitignore:
	@echo $(TESTS) | sed -e 's|test/||g' -e 's/ /\n/g' > test/.gitignore
