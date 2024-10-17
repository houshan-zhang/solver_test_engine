#!/bin/bash

DIR=${1}

# recursively searches for all files whose suffix is ".sh" in the "DIR" directory and authorizes them
find ${DIR} -type f -name "*.sh" -exec chmod +x {} \;
