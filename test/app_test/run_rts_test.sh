#!/usr/bin/env ksh
# vim: ts=4 sw=4 noet :
#==================================================================================
#    Copyright (c) 2019-2020 Nokia
#    Copyright (c) 2018-2020 AT&T Intellectual Property.
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
#	Mnemonic:	run_rts_test.ksh
#	Abstract:	This is a simple script to set up and run the basic send/receive
#				processes for some library validation on top of nng. This
#				particular test starts several senders and one receiver. All messages
#				go to the receiver and an ack is sent back to the sending process.
#				Each sender puts a tag into the message allowing it to verify that
#				all messages received were 'acks' to the ones it sent, and that no
#				rts messages were routed to the wrong sender.
#
#				Example command line:
#
#	Date:		15 May 2019
#	Author:		E. Scott Daniels
# ---------------------------------------------------------------------------------


# The sender and receivers are run asynch. Their exit statuses are captured in a
# file in order for the 'main' to pick them up easily.
# $1 is the instance so we can keep logs separate.
#
function run_sender {
	export RMR_RTG_SVC=$(( 9991 + $1 ))
	port=$((  43080 + $1 ))
	./sender${si} $nmsg $delay 5:6 $port
	echo $? >/tmp/PID$$.$1.src		# must communicate state back via file b/c asynch
}

# $1 is the instance so we can keep logs separate
function run_rcvr {
	typeset port

	port=4460
	export RMR_RTG_SVC=9990
	./receiver${si} $(( nmsg * nsenders ))  $port
	echo $? >/tmp/PID$$.rrc
}

#	Drop in a contrived route table. It should have only one entry which points
#	message type 0 to the receiver.  The sender must NOT be defined for a valid
#	tests (rts should not require the sendter be in the route thable).
#
function set_rt {
	cat <<endKat >rts.rt
		newrt | start
		mse |5 | -1 | localhost:4460
		newrt | end
endKat

}

# ---------------------------------------------------------

nmsg=10						# total number of messages to be exchanged (-n value changes)
delay=1000000				# microsec sleep between msg 1,000,000 == 1s
wait=1
rebuild=0
nopull=""
verbose=0
nsenders=3					# this is sane, but -s allows it to be set up
si=""

while [[ $1 == -* ]]
do
	case $1 in
		-B)	rebuild=1;;
		-b)	rebuild=1; nopull="nopull";;	# build without pulling
		-d)	delay=$2; shift;;
		-n)	nmsg=$2; shift;;
		-N) si="";;							# enable NNG tests (disable si)
		-s)	nsenders=$2; shift;;
		-S) si="_si";;						# enable SI95 tests
		-v)	verbose=1;;

		*)	echo "unrecognised option: $1"
			echo "usage: $0 [-B] [-d micor-sec-delay] [-M] [-n num-msgs] [-s nsenders] [-S]"
			echo "  -B forces a rebuild which will use .build"
			echo "  -M force test applications to be remade"
			echo "  -S build/test SI95 based binaries"
			exit 1
			;;
	esac

	shift
done

if (( verbose ))
then
	echo "2" >.verbose
	export RMR_VCTL_FILE=".verbose"
fi

if (( rebuild ))
then
	set -e
	$SHELL ./rebuild.ksh $nopull | read build_path
	set +e
else
	build_path=${BUILD_PATH:-"../../.build"}	# we prefer .build at the root level, but allow user option

	if [[ ! -d $build_path ]]
	then
		echo "cannot find build in: $build_path"
		echo "either create, and then build RMr, or set BUILD_PATH as an evironment var before running this"
		exit 1
	fi
fi

if [[ -z LD_LIBRARY_PATH ]]			# honour if cmake test sets it
then
	if [[ -d $build_path/lib64 ]]
	then
		export LD_LIBRARY_PATH=$build_path:$build_path/lib64:$LD_LIBRARY_PATH
	else
		export LD_LIBRARY_PATH=$build_path:$build_path/lib:$LD_LIBRARY_PATH
	fi
fi

export LIBRARY_PATH=$LD_LIBRARY_PATH
export RMR_SEED_RT=./rts.rt

set_rt 		# create the route table

if [[ ! -f ./sender${si} || ! -f ./receiver${si} ]]
then
	if ! make ./sender${si} ./receiver${si} >/dev/null 2>&1
	then
		echo "[FAIL] cannot find sender${si} and/or receiver${si} binary, and cannot make them.... humm?"
		exit 1
	fi
fi

run_rcvr &

sleep 2				# let receivers init so we don't shoot at an empty target
for (( i=0; i < nsenders; i++ ))		# start the receivers with an instance number
do
	run_sender $i &
done

wait


for (( i=0; i < nsenders; i++ ))		# collect return codes
do
	head -1 /tmp/PID$$.$i.src | read x
	(( src += x ))
done

head -1 /tmp/PID$$.rrc | read rrc

if (( !! (src + rrc) ))
then
	echo "[FAIL] sender rc=$src  receiver rc=$rrc"
else
	echo "[PASS] sender rc=$src  receiver rc=$rrc"
	rm -f multi.rt
fi

rm /tmp/PID$$.*
rm -f .verbose

exit $(( !! (src + rrc) ))

