GTESTER        = gtester
GTESTER_REPORT = gtester-report

# initialize variables for unconditional += appending
TEST_PROGS =

### testing rules

# test: run all tests in cwd and subdirs
test: test-cwd test-recurse
# test-cwd: run tests in cwd
test-cwd: ${TEST_PROGS}
	@[ -z "$(TEST_PROGS)" ] || { set -e; $(TESTS_ENVIRONMENT) ${GTESTER} --verbose ${TEST_PROGS}; }

# test-recurse: run tests in subdirs
test-recurse:
	@ for subdir in $(SUBDIRS) ; do \
	    test "$$subdir" = "." \
			-o "$$subdir" = "config" \
			-o "$$subdir" = "doc" \
			-o "$$subdir" = "manual" \
			-o "$$subdir" = "m4" \
			-o "$$subdir" = "po" \
			-o "$$subdir" = "tools" \
			|| \
	    ( cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) test ) || exit $? ; \
	  done
# test-report: run tests in subdirs and generate report
# perf-report: run tests in subdirs with -m perf and generate report
# full-report: like test-report: with -m perf and -m slow
test-report perf-report full-report:	${TEST_PROGS}
	@ ignore_logdir=true ; \
	  if test -z "$$GTESTER_LOGDIR" ; then \
	    GTESTER_LOGDIR=`mktemp -d "\`pwd\`/.testlogs-XXXXXX"`; export GTESTER_LOGDIR ; \
	    ignore_logdir=false ; \
	  fi ; \
	  for subdir in $(SUBDIRS) ; do \
	    test "$$subdir" = "." -o "$$subdir" = "po" -o "$$subdir" = "po-properties" || \
	    ( cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $? ; \
	  done ; \
	  test -z "${TEST_PROGS}" || { \
	    case $@ in \
	    test-report) test_options="-k";; \
	    perf-report) test_options="-k -m=perf";; \
	    full-report) test_options="-k -m=perf -m=slow";; \
	    esac ; \
	    set -e; \
	    if test -z "$$GTESTER_LOGDIR" ; then \
	      ${GTESTER} --verbose $$test_options -o test-report.xml ${TEST_PROGS} ; \
	    elif test -n "${TEST_PROGS}" ; then \
	      ${GTESTER} --verbose $$test_options -o `mktemp "$$GTESTER_LOGDIR/log-XXXXXX"` ${TEST_PROGS} ; \
	    fi ; \
	  }; \
	  $$ignore_logdir || { \
	    echo '<?xml version="1.0"?>' > $@.xml ; \
	    echo '<report-collection>'  >> $@.xml ; \
	    for lf in `ls -L "$$GTESTER_LOGDIR"/.` ; do \
	      sed '1,1s/^<?xml\b[^>?]*?>//' <"$$GTESTER_LOGDIR"/"$$lf" >> $@.xml ; \
	    done ; \
	    echo >> $@.xml ; \
	    echo '</report-collection>' >> $@.xml ; \
	    rm -rf "$$GTESTER_LOGDIR"/ ; \
	    ${GTESTER_REPORT} --version 2>/dev/null 1>&2 ; test "$$?" != 0 || ${GTESTER_REPORT} $@.xml >$@.html ; \
	  }
.PHONY: test test-cwd test-recurse test-report perf-report full-report
# run make test-cwd as part of make check
check-local: test-cwd
