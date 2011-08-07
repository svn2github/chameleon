#############################################################################
#!
#! This is a tmake template for creating a makefile that invokes make in
#! sub directories - for Unix.
#!
#${
    StdInit();
    Project('MAKEFILE') || Project('MAKEFILE = Makefile');
    Project('TMAKE') || Project('TMAKE = tmake');
#$}
#!
# Makefile for building targets in sub directories.
# Generated by tmake at #$ Now();
#     Project: #$ Expand("PROJECT");
#    Template: #$ Expand("TEMPLATE");
#############################################################################

MAKEFILE=	#$ Expand("MAKEFILE");
TMAKE	=	#$ Expand("TMAKE");

SUBDIRS	=	#$ ExpandList("SUBDIRS");

all: $(SUBDIRS)

$(SUBDIRS): FORCE
	cd $@; $(MAKE)

#$ TmakeSelf();

tmake_all:
#${
	$text = "\t" . 'for i in $(SUBDIRS); do ( cd $$i ; $(TMAKE) $$i.pro -o $(MAKEFILE); grep "TEMPLATE.*subdirs" $$i.pro 2>/dev/null >/dev/null && $(MAKE) -f $(MAKEFILE) tmake ) ; done';
#$}

clean:
	for i in $(SUBDIRS); do ( cd $$i ; $(MAKE) clean ) ; done

FORCE:
