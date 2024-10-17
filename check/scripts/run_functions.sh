#!/bin/bash

# formates the time given in ${1} from seconds into HH:MM:SS
function formatTime() {
    TMP=$((${1}))
    RESULT=""
    DIVISORS=(60 60 24)
    for((i=0; i<=2; i++))
    do
        printf -v RESULT "%02d${RESULT}" $(expr ${TMP} % ${DIVISORS[i]})
        # separate the numbers by colons except for the last (HH hours)
        if test $i -lt 2
        then
            RESULT=":${RESULT}"
        fi
        TMP=`expr ${TMP} / ${DIVISORS[i]}`
    done
    if test ${TMP} -gt 0
    then
        RESULT=${TMP}-${RESULT}
    fi
    echo $RESULT
}

# removes the the path and the'.mps.gz' extension from the instance file in ${1}
function stripPathAndInstanceExtension() {
   echo $(basename $(echo $(basename $(echo $(basename ${1} ".gz")) ".mps")) ".lp")
}

# removes the path leading to the file in ${1}
function stripPath() {
    echo $(basename ${1})
}

# Replaces the "{INSTANCENAME}" with ${2} in the pattern ${1}.
function replaceInPattern() {
    local PATTERN=${1}
    local INSTANCENAME=${2}

    local RESULT=$(echo ${PATTERN} | sed "s/{INSTANCENAME}/${INSTANCENAME}/g")
    echo ${RESULT}
}
