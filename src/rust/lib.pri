CONFIG(debug, debug|release) {
	RUST_BUILD_DIR = $$PWD/../../target/debug
} else {
	RUST_BUILD_DIR = $$PWD/../../target/release
}

LIBS += -L$$RUST_BUILD_DIR -lp-8 -ldl
PRE_TARGETDEPS += $$RUST_BUILD_DIR/libp-8.a
