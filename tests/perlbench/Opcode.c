/*
 * This file was generated automatically by xsubpp version 1.9508 from the
 * contents of Opcode.xs. Do not edit this file, edit Opcode.xs instead.
 *
 *	ANY CHANGES MADE HERE WILL BE LOST!
 *
 */

/* #line 1 "Opcode.xs" */
#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* PL_maxo shouldn't differ from MAXO but leave room anyway (see BOOT:)	*/
#define OP_MASK_BUF_SIZE (MAXO + 100)

/* XXX op_named_bits and opset_all are never freed */
#define MY_CXT_KEY "Opcode::_guts" XS_VERSION

typedef struct {
    HV *	x_op_named_bits;	/* cache shared for whole process */
    SV *	x_opset_all;		/* mask with all bits set	*/
    IV		x_opset_len;		/* length of opmasks in bytes	*/
    int		x_opcode_debug;
} my_cxt_t;

START_MY_CXT

#define op_named_bits		(MY_CXT.x_op_named_bits)
#define opset_all		(MY_CXT.x_opset_all)
#define opset_len		(MY_CXT.x_opset_len)
#define opcode_debug		(MY_CXT.x_opcode_debug)

static SV  *new_opset (pTHX_ SV *old_opset);
static int  verify_opset (pTHX_ SV *opset, int fatal);
static void set_opset_bits (pTHX_ char *bitmap, SV *bitspec, int on, char *opname);
static void put_op_bitspec (pTHX_ char *optag,  STRLEN len, SV *opset);
static SV  *get_op_bitspec (pTHX_ char *opname, STRLEN len, int fatal);


/* Initialise our private op_named_bits HV.
 * It is first loaded with the name and number of each perl operator.
 * Then the builtin tags :none and :all are added.
 * Opcode.pm loads the standard optags from __DATA__
 * XXX leak-alert: data allocated here is never freed, call this
 *     at most once
 */

static void
op_names_init(pTHX)
{
    int i;
    STRLEN len;
    char **op_names;
    char *bitmap;
    dMY_CXT;

    op_named_bits = newHV();
    op_names = get_op_names();
    for(i=0; i < PL_maxo; ++i) {
	SV *sv;
	sv = newSViv(i);
	SvREADONLY_on(sv);
	hv_store(op_named_bits, op_names[i], strlen(op_names[i]), sv, 0);
    }

    put_op_bitspec(aTHX_ ":none",0, sv_2mortal(new_opset(aTHX_ Nullsv)));

    opset_all = new_opset(aTHX_ Nullsv);
    bitmap = SvPV(opset_all, len);
    i = len-1; /* deal with last byte specially, see below */
    while(i-- > 0)
	bitmap[i] = (char)0xFF;
    /* Take care to set the right number of bits in the last byte */
    bitmap[len-1] = (PL_maxo & 0x07) ? ~(0xFF << (PL_maxo & 0x07)) : 0xFF;
    put_op_bitspec(aTHX_ ":all",0, opset_all); /* don't mortalise */
}


/* Store a new tag definition. Always a mask.
 * The tag must not already be defined.
 * SV *mask is copied not referenced.
 */

static void
put_op_bitspec(pTHX_ char *optag, STRLEN len, SV *mask)
{
    SV **svp;
    dMY_CXT;

    verify_opset(aTHX_ mask,1);
    if (!len)
	len = strlen(optag);
    svp = hv_fetch(op_named_bits, optag, len, 1);
    if (SvOK(*svp))
	croak("Opcode tag \"%s\" already defined", optag);
    sv_setsv(*svp, mask);
    SvREADONLY_on(*svp);
}



/* Fetch a 'bits' entry for an opname or optag (IV/PV).
 * Note that we return the actual entry for speed.
 * Always sv_mortalcopy() if returing it to user code.
 */

static SV *
get_op_bitspec(pTHX_ char *opname, STRLEN len, int fatal)
{
    SV **svp;
    dMY_CXT;

    if (!len)
	len = strlen(opname);
    svp = hv_fetch(op_named_bits, opname, len, 0);
    if (!svp || !SvOK(*svp)) {
	if (!fatal)
	    return Nullsv;
	if (*opname == ':')
	    croak("Unknown operator tag \"%s\"", opname);
	if (*opname == '!')	/* XXX here later, or elsewhere? */
	    croak("Can't negate operators here (\"%s\")", opname);
	if (isALPHA(*opname))
	    croak("Unknown operator name \"%s\"", opname);
	croak("Unknown operator prefix \"%s\"", opname);
    }
    return *svp;
}



static SV *
new_opset(pTHX_ SV *old_opset)
{
    SV *opset;
    dMY_CXT;

    if (old_opset) {
	verify_opset(aTHX_ old_opset,1);
	opset = newSVsv(old_opset);
    }
    else {
	opset = NEWSV(1156, opset_len);
	Zero(SvPVX(opset), opset_len + 1, char);
	SvCUR_set(opset, opset_len);
	(void)SvPOK_only(opset);
    }
    /* not mortalised here */
    return opset;
}


static int
verify_opset(pTHX_ SV *opset, int fatal)
{
    char *err = Nullch;
    dMY_CXT;

    if      (!SvOK(opset))              err = "undefined";
    else if (!SvPOK(opset))             err = "wrong type";
    else if (SvCUR(opset) != (STRLEN)opset_len) err = "wrong size";
    if (err && fatal) {
	croak("Invalid opset: %s", err);
    }
    return !err;
}


static void
set_opset_bits(pTHX_ char *bitmap, SV *bitspec, int on, char *opname)
{
    dMY_CXT;

    if (SvIOK(bitspec)) {
	int myopcode = SvIV(bitspec);
	int offset = myopcode >> 3;
	int bit    = myopcode & 0x07;
	if (myopcode >= PL_maxo || myopcode < 0)
	    croak("panic: opcode \"%s\" value %d is invalid", opname, myopcode);
	if (opcode_debug >= 2)
	    warn("set_opset_bits bit %2d (off=%d, bit=%d) %s %s\n",
			myopcode, offset, bit, opname, (on)?"on":"off");
	if (on)
	    bitmap[offset] |= 1 << bit;
	else
	    bitmap[offset] &= ~(1 << bit);
    }
    else if (SvPOK(bitspec) && SvCUR(bitspec) == (STRLEN)opset_len) {

	STRLEN len;
	char *specbits = SvPV(bitspec, len);
	if (opcode_debug >= 2)
	    warn("set_opset_bits opset %s %s\n", opname, (on)?"on":"off");
	if (on) 
	    while(len-- > 0) bitmap[len] |=  specbits[len];
	else
	    while(len-- > 0) bitmap[len] &= ~specbits[len];
    }
    else
	croak("panic: invalid bitspec for \"%s\" (type %u)",
		opname, (unsigned)SvTYPE(bitspec));
}


static void
opmask_add(pTHX_ SV *opset)	/* THE ONLY FUNCTION TO EDIT PL_op_mask ITSELF	*/
{
    int i,j;
    char *bitmask;
    STRLEN len;
    int myopcode = 0;
    dMY_CXT;

    verify_opset(aTHX_ opset,1);		/* croaks on bad opset	*/

    if (!PL_op_mask)		/* caller must ensure PL_op_mask exists	*/
	croak("Can't add to uninitialised PL_op_mask");

    /* OPCODES ALREADY MASKED ARE NEVER UNMASKED. See opmask_addlocal()	*/

    bitmask = SvPV(opset, len);
    for (i=0; i < opset_len; i++) {
	U16 bits = bitmask[i];
	if (!bits) {	/* optimise for sparse masks */
	    myopcode += 8;
	    continue;
	}
	for (j=0; j < 8 && myopcode < PL_maxo; )
	    PL_op_mask[myopcode++] |= bits & (1 << j++);
    }
}

static void
opmask_addlocal(pTHX_ SV *opset, char *op_mask_buf) /* Localise PL_op_mask then opmask_add() */
{
    char *orig_op_mask = PL_op_mask;
    dMY_CXT;

    SAVEVPTR(PL_op_mask);
    /* XXX casting to an ordinary function ptr from a member function ptr
     * is disallowed by Borland
     */
    if (opcode_debug >= 2)
	SAVEDESTRUCTOR((void(*)(void*))Perl_warn,"PL_op_mask restored");
    PL_op_mask = &op_mask_buf[0];
    if (orig_op_mask)
	Copy(orig_op_mask, PL_op_mask, PL_maxo, char);
    else
	Zero(PL_op_mask, PL_maxo, char);
    opmask_add(aTHX_ opset);
}



/* #line 258 "Opcode.c" */
XS(XS_Opcode__safe_pkg_prep); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode__safe_pkg_prep)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "Usage: Opcode::_safe_pkg_prep(Package)");
    SP -= items;
    {
	char *	Package = (char *)SvPV_nolen(ST(0));
/* #line 266 "Opcode.xs" */
    HV *hv; 
    ENTER;

    hv = gv_stashpv(Package, GV_ADDWARN); /* should exist already	*/

    if (strNE(HvNAME(hv),"main")) {
        Safefree(HvNAME(hv));         
        HvNAME(hv) = savepv("main"); /* make it think it's in main:: */
        hv_store(hv,"_",1,(SV *)PL_defgv,0);  /* connect _ to global */
        SvREFCNT_inc((SV *)PL_defgv);  /* want to keep _ around! */
    }
    LEAVE;
/* #line 281 "Opcode.c" */
	PUTBACK;
	return;
    }
}

XS(XS_Opcode__safe_call_sv); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode__safe_call_sv)
{
    dXSARGS;
    if (items != 3)
	Perl_croak(aTHX_ "Usage: Opcode::_safe_call_sv(Package, mask, codesv)");
    SP -= items;
    {
	char *	Package = (char *)SvPV_nolen(ST(0));
	SV *	mask = ST(1);
	SV *	codesv = ST(2);
/* #line 289 "Opcode.xs" */
    char op_mask_buf[OP_MASK_BUF_SIZE];
    GV *gv;
    HV *dummy_hv;

    ENTER;

    opmask_addlocal(aTHX_ mask, op_mask_buf);

    save_aptr(&PL_endav);
    PL_endav = (AV*)sv_2mortal((SV*)newAV()); /* ignore END blocks for now	*/

    save_hptr(&PL_defstash);		/* save current default stash	*/
    /* the assignment to global defstash changes our sense of 'main'	*/
    PL_defstash = gv_stashpv(Package, GV_ADDWARN); /* should exist already	*/

    save_hptr(&PL_curstash);
    PL_curstash = PL_defstash;

    /* defstash must itself contain a main:: so we'll add that now	*/
    /* take care with the ref counts (was cause of long standing bug)	*/
    /* XXX I'm still not sure if this is right, GV_ADDWARN should warn!	*/
    gv = gv_fetchpv("main::", GV_ADDWARN, SVt_PVHV);
    sv_free((SV*)GvHV(gv));
    GvHV(gv) = (HV*)SvREFCNT_inc(PL_defstash);

    /* %INC must be clean for use/require in compartment */
    dummy_hv = save_hash(PL_incgv);
    GvHV(PL_incgv) = (HV*)SvREFCNT_inc(GvHV(gv_HVadd(gv_fetchpv("INC",TRUE,SVt_PVHV))));

    PUSHMARK(SP);
    call_sv(codesv, GIMME|G_EVAL|G_KEEPERR); /* use callers context */ /* SPEC CPU */
    sv_free( (SV *) dummy_hv);  /* get rid of what save_hash gave us*/
    SPAGAIN; /* for the PUTBACK added by xsubpp */
    LEAVE;
/* #line 333 "Opcode.c" */
	PUTBACK;
	return;
    }
}

XS(XS_Opcode_verify_opset); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_verify_opset)
{
    dXSARGS;
    if (items < 1 || items > 2)
	Perl_croak(aTHX_ "Usage: Opcode::verify_opset(opset, fatal = 0)");
    {
	SV *	opset = ST(0);
	int	fatal;
	int	RETVAL;
	dXSTARG;

	if (items < 2)
	    fatal = 0;
	else {
	    fatal = (int)SvIV(ST(1));
	}
/* #line 330 "Opcode.xs" */
    RETVAL = verify_opset(aTHX_ opset,fatal);
/* #line 358 "Opcode.c" */
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}

XS(XS_Opcode_invert_opset); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_invert_opset)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "Usage: Opcode::invert_opset(opset)");
    {
	SV *	opset = ST(0);
/* #line 338 "Opcode.xs" */
    {
    char *bitmap;
    dMY_CXT;
    STRLEN len = opset_len;

    opset = sv_2mortal(new_opset(aTHX_ opset));	/* verify and clone opset */
    bitmap = SvPVX(opset);
    while(len-- > 0)
	bitmap[len] = ~bitmap[len];
    /* take care of extra bits beyond PL_maxo in last byte	*/
    if (PL_maxo & 07)
	bitmap[opset_len-1] &= ~(0xFF << (PL_maxo & 0x07));
    }
    ST(0) = opset;
/* #line 387 "Opcode.c" */
    }
    XSRETURN(1);
}

XS(XS_Opcode_opset_to_ops); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_opset_to_ops)
{
    dXSARGS;
    if (items < 1 || items > 2)
	Perl_croak(aTHX_ "Usage: Opcode::opset_to_ops(opset, desc = 0)");
    SP -= items;
    {
	SV *	opset = ST(0);
	int	desc;

	if (items < 2)
	    desc = 0;
	else {
	    desc = (int)SvIV(ST(1));
	}
/* #line 359 "Opcode.xs" */
    {
    STRLEN len;
    int i, j, myopcode;
    char *bitmap = SvPV(opset, len);
    char **names = (desc) ? get_op_descs() : get_op_names();
    dMY_CXT;

    verify_opset(aTHX_ opset,1);
    for (myopcode=0, i=0; i < opset_len; i++) {
	U16 bits = bitmap[i];
	for (j=0; j < 8 && myopcode < PL_maxo; j++, myopcode++) {
	    if ( bits & (1 << j) )
		XPUSHs(sv_2mortal(newSVpv(names[myopcode], 0)));
	}
    }
    }
/* #line 425 "Opcode.c" */
	PUTBACK;
	return;
    }
}

XS(XS_Opcode_opset); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_opset)
{
    dXSARGS;
    {
/* #line 380 "Opcode.xs" */
    int i;
    SV *bitspec, *opset;
    char *bitmap;
    STRLEN len, on;

    opset = sv_2mortal(new_opset(aTHX_ Nullsv));
    bitmap = SvPVX(opset);
    for (i = 0; i < items; i++) {
	char *opname;
	on = 1;
	if (verify_opset(aTHX_ ST(i),0)) {
	    opname = "(opset)";
	    bitspec = ST(i);
	}
	else {
	    opname = SvPV(ST(i), len);
	    if (*opname == '!') { on=0; ++opname;--len; }
	    bitspec = get_op_bitspec(aTHX_ opname, len, 1);
	}
	set_opset_bits(aTHX_ bitmap, bitspec, on, opname);
    }
    ST(0) = opset;
/* #line 459 "Opcode.c" */
    }
    XSRETURN(1);
}

#define PERMITING  (ix == 0 || ix == 1)
#define ONLY_THESE (ix == 0 || ix == 2)
XS(XS_Opcode_permit_only); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_permit_only)
{
    dXSARGS;
    dXSI32;
    if (items < 1)
       Perl_croak(aTHX_ "Usage: %s(safe, ...)", GvNAME(CvGV(cv)));
    {
	SV *	safe = ST(0);
/* #line 415 "Opcode.xs" */
    int i, on;
    SV *bitspec, *mask;
    char *bitmap, *opname;
    STRLEN len;
    dMY_CXT;

    if (!SvROK(safe) || !SvOBJECT(SvRV(safe)) || SvTYPE(SvRV(safe))!=SVt_PVHV)
	croak("Not a Safe object");
    mask = *hv_fetch((HV*)SvRV(safe), "Mask",4, 1);
    if (ONLY_THESE)	/* *_only = new mask, else edit current	*/
	sv_setsv(mask, sv_2mortal(new_opset(aTHX_ PERMITING ? opset_all : Nullsv)));
    else
	verify_opset(aTHX_ mask,1); /* croaks */
    bitmap = SvPVX(mask);
    for (i = 1; i < items; i++) {
	on = PERMITING ? 0 : 1;		/* deny = mask bit on	*/
	if (verify_opset(aTHX_ ST(i),0)) {	/* it's a valid mask	*/
	    opname = "(opset)";
	    bitspec = ST(i);
	}
	else {				/* it's an opname/optag	*/
	    opname = SvPV(ST(i), len);
	    /* invert if op has ! prefix (only one allowed)	*/
	    if (*opname == '!') { on = !on; ++opname; --len; }
	    bitspec = get_op_bitspec(aTHX_ opname, len, 1); /* croaks */
	}
	set_opset_bits(aTHX_ bitmap, bitspec, on, opname);
    }
    ST(0) = &PL_sv_yes;
/* #line 505 "Opcode.c" */
    }
    XSRETURN(1);
}

XS(XS_Opcode_opdesc); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_opdesc)
{
    dXSARGS;
   PERL_UNUSED_VAR(ax); /* -Wall */
    SP -= items;
    {
/* #line 450 "Opcode.xs" */
    int i, myopcode;
    STRLEN len;
    SV **args;
    char **op_desc = get_op_descs(); 
    dMY_CXT;

    /* copy args to a scratch area since we may push output values onto	*/
    /* the stack faster than we read values off it if masks are used.	*/
    args = (SV**)SvPVX(sv_2mortal(newSVpvn((char*)&ST(0), items*sizeof(SV*))));
    for (i = 0; i < items; i++) {
	char *opname = SvPV(args[i], len);
	SV *bitspec = get_op_bitspec(aTHX_ opname, len, 1);
	if (SvIOK(bitspec)) {
	    myopcode = SvIV(bitspec);
	    if (myopcode < 0 || myopcode >= PL_maxo)
		croak("panic: opcode %d (%s) out of range",myopcode,opname);
	    XPUSHs(sv_2mortal(newSVpv(op_desc[myopcode], 0)));
	}
	else if (SvPOK(bitspec) && SvCUR(bitspec) == (STRLEN)opset_len) {
	    int b, j;
	    STRLEN n_a;
	    char *bitmap = SvPV(bitspec,n_a);
	    myopcode = 0;
	    for (b=0; b < opset_len; b++) {
		U16 bits = bitmap[b];
		for (j=0; j < 8 && myopcode < PL_maxo; j++, myopcode++)
		    if (bits & (1 << j))
			XPUSHs(sv_2mortal(newSVpv(op_desc[myopcode], 0)));
	    }
	}
	else
	    croak("panic: invalid bitspec for \"%s\" (type %u)",
		opname, (unsigned)SvTYPE(bitspec));
    }
/* #line 552 "Opcode.c" */
	PUTBACK;
	return;
    }
}

XS(XS_Opcode_define_optag); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_define_optag)
{
    dXSARGS;
    if (items != 2)
	Perl_croak(aTHX_ "Usage: Opcode::define_optag(optagsv, mask)");
    {
	SV *	optagsv = ST(0);
	SV *	mask = ST(1);
/* #line 491 "Opcode.xs" */
    STRLEN len;
    char *optag = SvPV(optagsv, len);

    put_op_bitspec(aTHX_ optag, len, mask); /* croaks */
    ST(0) = &PL_sv_yes;
/* #line 573 "Opcode.c" */
    }
    XSRETURN(1);
}

XS(XS_Opcode_empty_opset); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_empty_opset)
{
    dXSARGS;
    if (items != 0)
	Perl_croak(aTHX_ "Usage: Opcode::empty_opset()");
    {
/* #line 501 "Opcode.xs" */
    ST(0) = sv_2mortal(new_opset(aTHX_ Nullsv));
/* #line 587 "Opcode.c" */
    }
    XSRETURN(1);
}

XS(XS_Opcode_full_opset); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_full_opset)
{
    dXSARGS;
    if (items != 0)
	Perl_croak(aTHX_ "Usage: Opcode::full_opset()");
    {
/* #line 506 "Opcode.xs" */
    dMY_CXT;
    ST(0) = sv_2mortal(new_opset(aTHX_ opset_all));
/* #line 602 "Opcode.c" */
    }
    XSRETURN(1);
}

XS(XS_Opcode_opmask_add); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_opmask_add)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "Usage: Opcode::opmask_add(opset)");
    {
	SV *	opset = ST(0);
/* #line 513 "Opcode.xs" */
    if (!PL_op_mask)
	Newz(0, PL_op_mask, PL_maxo, char);
/* #line 618 "Opcode.c" */
/* #line 516 "Opcode.xs" */
    opmask_add(aTHX_ opset);
/* #line 621 "Opcode.c" */
    }
    XSRETURN_EMPTY;
}

XS(XS_Opcode_opcodes); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_opcodes)
{
    dXSARGS;
    if (items != 0)
	Perl_croak(aTHX_ "Usage: Opcode::opcodes()");
   PERL_UNUSED_VAR(ax); /* -Wall */
    SP -= items;
    {
/* #line 521 "Opcode.xs" */
    if (GIMME == G_ARRAY) {
	croak("opcodes in list context not yet implemented"); /* XXX */
    }
    else {
	XPUSHs(sv_2mortal(newSViv(PL_maxo)));
    }
/* #line 642 "Opcode.c" */
	PUTBACK;
	return;
    }
}

XS(XS_Opcode_opmask); /* prototype to pass -Wmissing-prototypes */
XS(XS_Opcode_opmask)
{
    dXSARGS;
    if (items != 0)
	Perl_croak(aTHX_ "Usage: Opcode::opmask()");
    {
/* #line 531 "Opcode.xs" */
    ST(0) = sv_2mortal(new_opset(aTHX_ Nullsv));
    if (PL_op_mask) {
	char *bitmap = SvPVX(ST(0));
	int myopcode;
	for(myopcode=0; myopcode < PL_maxo; ++myopcode) {
	    if (PL_op_mask[myopcode])
		bitmap[myopcode >> 3] |= 1 << (myopcode & 0x07);
	}
    }
/* #line 665 "Opcode.c" */
    }
    XSRETURN(1);
}

#ifdef __cplusplus
extern "C"
#endif
XS(boot_Opcode); /* prototype to pass -Wmissing-prototypes */
XS(boot_Opcode)
{
    dXSARGS;
    char* file = __FILE__;

    XS_VERSION_BOOTCHECK ;

    {
        CV * cv ;

        newXSproto("Opcode::_safe_pkg_prep", XS_Opcode__safe_pkg_prep, file, "$");
        newXSproto("Opcode::_safe_call_sv", XS_Opcode__safe_call_sv, file, "$$$");
        newXSproto("Opcode::verify_opset", XS_Opcode_verify_opset, file, "$;$");
        newXSproto("Opcode::invert_opset", XS_Opcode_invert_opset, file, "$");
        newXSproto("Opcode::opset_to_ops", XS_Opcode_opset_to_ops, file, "$;$");
        newXSproto("Opcode::opset", XS_Opcode_opset, file, ";@");
        cv = newXS("Opcode::permit_only", XS_Opcode_permit_only, file);
        XSANY.any_i32 = 0 ;
        sv_setpv((SV*)cv, "$;@") ;
        cv = newXS("Opcode::deny", XS_Opcode_permit_only, file);
        XSANY.any_i32 = 3 ;
        sv_setpv((SV*)cv, "$;@") ;
        cv = newXS("Opcode::deny_only", XS_Opcode_permit_only, file);
        XSANY.any_i32 = 2 ;
        sv_setpv((SV*)cv, "$;@") ;
        cv = newXS("Opcode::permit", XS_Opcode_permit_only, file);
        XSANY.any_i32 = 1 ;
        sv_setpv((SV*)cv, "$;@") ;
        newXSproto("Opcode::opdesc", XS_Opcode_opdesc, file, ";@");
        newXSproto("Opcode::define_optag", XS_Opcode_define_optag, file, "$$");
        newXSproto("Opcode::empty_opset", XS_Opcode_empty_opset, file, "");
        newXSproto("Opcode::full_opset", XS_Opcode_full_opset, file, "");
        newXSproto("Opcode::opmask_add", XS_Opcode_opmask_add, file, "$");
        newXSproto("Opcode::opcodes", XS_Opcode_opcodes, file, "");
        newXSproto("Opcode::opmask", XS_Opcode_opmask, file, "");
    }

    /* Initialisation Section */

/* #line 253 "Opcode.xs" */
{
    MY_CXT_INIT;
    assert(PL_maxo < OP_MASK_BUF_SIZE);
    opset_len = (PL_maxo + 7) / 8;
    if (opcode_debug >= 1)
	warn("opset_len %ld\n", (long)opset_len);
    op_names_init(aTHX);
}

/* #line 723 "Opcode.c" */

    /* End of Initialisation Section */

    XSRETURN_YES;
}

