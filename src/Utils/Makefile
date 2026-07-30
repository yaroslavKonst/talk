.PHONY: all

all: $(BUILD_DIR)/Utils/EndiannessChecker

$(BUILD_DIR)/Utils/EndiannessChecker: EndiannessChecker.cpp | $(BUILD_DIR)/Utils
	$(CXX) $(STATIC_FLAG) -o $@ $<

$(BUILD_DIR)/Utils:
	mkdir -p $@
