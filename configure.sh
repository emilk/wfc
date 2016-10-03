set -eu

if [ ! -d "libs/emilib" ]; then
	git clone git@github.com:emilk/emilib.git libs/emilib
fi
