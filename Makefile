PREPCMD = sed -i"" "/\/\*\*\[BEGIN-REMOVE-FOR-STUDENTS\]\*\*\//,/\/\*\*\[END-REMOVE-FOR-STUDENTS\]\*\*\//d"

default: usage

part1:
	$(MAKE) -C part1

part2:
	$(MAKE) -C part2

clean_part1:
	$(MAKE) clean -C part1

clean_part2:
	$(MAKE) clean -C part2

protos:
	$(MAKE) protos -C part1
	$(MAKE) protos -C part2

protos_part1:
	$(MAKE) protos -C part1

protos_part2:
	$(MAKE) protos -C part2

clean_all:
	$(MAKE) clean_all -C part1
	$(MAKE) clean_all -C part2

clean_protos:
	$(MAKE) clean_protos -C part1
	$(MAKE) clean_protos -C part2

the_works: clean_all protos part1 part2

SREPO_DIR = ./student-repo/


.PHONY: part1
.PHONY: part2
.PHONY: part1_clean
.PHONY: part2_clean
.PHONY: clean_all
.PHONY: clean_protos
.PHONY: protos
.PHONY: the_works

usage:
	@echo
	@echo "USAGE: make part1 | make part2"
	@echo
	@echo "Or, cd into one of the part1 or part2 directories and run make from there."
	@echo
	@echo "Additional options:"
	@echo
	@echo "- make protos - generates the protobuf classes"
	@echo "- make part1_clean - cleans part1"
	@echo "- make part2_clean - cleans part2"
	@echo "- make clean_all - cleans all projects and protobuf files"
	@echo "- make clean_protos - cleans protobuf files, including generated classes"
	@echo
	@echo "Options are also available inside the directories: "
	@echo
	@echo "- make - makes the current part"
	@echo "- make protos - generates the protobuf classes"
	@echo "- make clean - cleans the current project"
	@echo "- make clean_all - cleans all projects and protobuf files"
	@echo "- make clean_protos - cleans protobuf files, including generated classes"
	@echo

