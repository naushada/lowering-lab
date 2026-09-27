# Top level: run a target across every chapter.
#   make            build every chapter's assembly
#   make tables     the cost table for every chapter, end to end
#   make run        run every chapter that has something to execute
#   make clean      remove all generated output
CHAPTERS := $(sort $(wildcard [0-9][0-9]-*))

.PHONY: all tables run remarks clean list $(CHAPTERS)

all: $(CHAPTERS)

$(CHAPTERS):
	@echo "=== $@"
	@$(MAKE) --no-print-directory -C $@ asm

tables:
	@for d in $(CHAPTERS); do echo "############ $$d"; $(MAKE) --no-print-directory -C $$d table; done

run:
	@for d in $(CHAPTERS); do $(MAKE) --no-print-directory -C $$d run; done

remarks:
	@for d in $(CHAPTERS); do echo "############ $$d"; $(MAKE) --no-print-directory -C $$d remarks; done

clean:
	@for d in $(CHAPTERS); do $(MAKE) --no-print-directory -C $$d clean; done
	@echo "cleaned"

list:
	@for d in $(CHAPTERS); do printf "%-32s %s\n" "$$d" "`sed -n '3p' $$d/note.md`"; done
