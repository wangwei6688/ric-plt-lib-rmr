#!/usr/bin/env ksh
# :vi ts=4 sw=4 noet :
#==================================================================================
#    Copyright (c) 2019 Nokia
#    Copyright (c) 2018-2019 AT&T Intellectual Property.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#==================================================================================
#

# ---------------------------------------------------------------------------------
#	Mnemonic:	run_app_test.ksh
#	Abstract:	This is a simple script to set up and run the basic send/receive
#				processes for some library validation on top of nano/nng.
#				It should be possible to clone the repo, switch to this directory
#				and execute  'ksh run -B'  which will build RMr, make the sender and
#				recevier then  run the basic test.
#
#				Example command line:
#					ksh ./run		# default 10 messages at 1 msg/sec
#					ksh ./run -N    # default but with nanomsg lib
#					ksh ./run -d 100 -n 10000 # send 10k messages with 100ms delay between
#
#	Date:		22 April 2019
#	Author:		E. Scott Daniels
# ---------------------------------------------------------------------------------


# The sender and receiver are run asynch. Their exit statuses are captured in a
# file in order for the 'main' to pick them up easily.
#
function run_sender {
	if (( $nano_sender ))
	then
		./sender_nano $nmsg $delay
	else
		./sender $nmsg $delay
	fi
	echo $? >/tmp/PID$$.src		# must communicate state back via file b/c asynch
}

function run_rcvr {
	if (( $nano_receiver ))
	then
		./receiver_nano $nmsg
	else
		./receiver $nmsg
	fi
	echo $? >/tmp/PID$$.rrc
}

# ---------------------------------------------------------

if [[ ! -f local.rt ]]		# we need the real host name in the local.rt; build one from mask if not there
then
	hn=$(hostname)
	sed "s!%%hostname%%!$hn!" rt.mask >local.rt
fi

nmsg=10						# total number of messages to be exchanged (-n value changes)
delay=1000000				# microsec sleep between msg 1,000,000 == 1s
nano_sender=0				# start nano version if set (-N)
nano_receiver=0
wait=1
rebuild=0

while [[ $1 == -* ]]
do
	case $1 in 
		-B)	rebuild=1;;
		-d)	delay=$2; shift;;
		-N)	nano_sender=1
			nano_receiver=1
			;;
		-n)	nmsg=$2; shift;;

		*)	echo "unrecognised option: $1"
			echo "usage: $0 [-B] [-d micor-sec-delay] [-N] [-n num-msgs]"
			echo "  -B forces a rebuild which will use .build"
			exit 1
			;;
	esac

	shift
done

if (( rebuild )) 
then
	build_path=../../.build

	(
		set -e
		mkdir -p $build_path
		cd ${build_path%/*}				# cd barfs on ../../.build, so we do this
		cd ${build_path##*/}
		cmake ..
		make package
	)
	if (( $? != 0 ))
	then
		echo "build failed"
		exit 1
	fi
else
	build_path=${BUILD_PATH:-"../../.build"}	# we prefer .build at the root level, but allow user option

	if [[ ! -d $build_path ]]
	then
		echo "cannot find build in: $build_path"
		echo "either create, and then build RMr, or set BUILD_PATH as an evironment var before running this"
		exit 1
	fi
fi

export LD_LIBRARY_PATH=$build_path:$build_path/lib
export LIBRARY_PATH=$LD_LIBRARY_PATH
export RMR_SEED_RT=${RMR_SEED_RT:-./local.rt}		# allow easy testing with different rt


if [[ ! -f ./sender ]]
then
	if ! make >/dev/null 2>&1
	then
		echo "[FAIL] cannot find sender binary, and cannot make it.... humm?"
		exit 1
	fi
fi

run_rcvr &
run_sender &

wait
head -1 /tmp/PID$$.rrc | read rrc
head -1 /tmp/PID$$.src | read src

if (( !! (src + rrc) ))
then
	echo "[FAIL] sender rc=$src  receiver rc=$rrc"
else
	echo "[PASS] sender rc=$src  receiver rc=$rrc"
fi

rm /tmp/PID$$.*

exit $(( !! (src + rrc) ))
