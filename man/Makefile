BINS = clang-mutate
MANS = $(BINS:=.1.gz)

all: $(MANS)

fields.md:
	echo "?fields md" >  gen_field_help
	echo "quit"       >> gen_field_help
	touch empty.c
	cat gen_field_help | clang-mutate -interactive -silent empty.c -- > $@
	rm empty.c gen_field_help

clang-mutate.md: clang-mutate.template.md fields.md
	sed -e "/INCLUDE_FIELDS_MD/{r fields.md" -e "d}" clang-mutate.template.md > $@

%.1.gz: %.md
	pandoc -s -t man $< -o $<.tmp
	gzip -9 < $<.tmp > $@

clean:
	rm -f $(MANS) $(BINS:=.md.tmp) fields.md clang-mutate.md
