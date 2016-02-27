#!/usr/bin/env python3
# -*- coding: ascii -*-

import sys
import os
import struct
import re

"""
We want to have syntax like this:

Number:
    $1512
    or
    0x1242

Register:
    %ret

Memory at...
    Register:
        *(ret)
    Offset:
        *(0x1252)
    Register+Offset:
        *(ret+0x1252)

Addition of:
    Register and Register:
        %c1, %c2
    Register+Literal and Register:
        %c1+0x1252, %c2
    Literal and Register:
        0x1252, %c2

"""

registers = {
    "ins": 0x0,
    "stk": 0x1,
    "bse": 0x2,
    "ret": 0x3,
    "c1": 0x4,
    "c2": 0x5,
    "c3": 0x6,
    "c4": 0x7,
    "c5": 0x8,
    "c6": 0x9,
    "s1": 0xA,
    "s2": 0xB,
    "s3": 0xC,
    "s4": 0xD,
    "s5": 0xE,
    "null": 0xF, # TODO: Throw error on use?
}

class AssembleError(Exception):
    def __init__(self, linenum, message):
        self.linenum = linenum
        self.message = message

    def __str__(self):
        return "Error(line %d): %s"%(self.linenum,self.message)

icodes = {
    "control": 0x0,
    "arith": 0x1,
    "move": 0x2,
    "stack": 0x3,
    "branch": 0x4,
    "conditional": 0x5
}

# Order in these sub lists specify the ifun code.
ifuns = {
    "control": [
        "halt",
        "pause",
        "nop"
    ],
    "arith": [
        "add",
        "sub",
        "mul",
        "div",
        "divs",
        "lsh",
        "rsh",
        "rshs",
        "and",
        "or",
        "xor",
        "not",
    ],
    "move": [
        "irmov",
        "mrmov",
        "rrmov",
        "rmmov"
    ],
    "stack": [
        "push",
        "pop"
    ],
    "branch": [
        "call",
        "ret",
        "jmp",
        "jif",
        "syscall"
    ],
    "conditional": [
        "lt",
        "lts",
        "lte",
        "ltes",
        "eq",
        "neq"
    ]
}

instruction_types = {}
for ins_type in ifuns.keys():
    for ins in ifuns[ins_type]:
        instruction_types[ins] = ins_type

instruction_types["mov"] = "move"

#Note for negative d's, it has to look like: $-100 or -0x231A. The + always has to be there.
memcase = re.compile(r"^\*\((.+?)\)") #Matches memory expressions

regre = r"%(?P<reg>\w+)"
litre = r"(?P<dtype>-?0x|\$-?)(?P<d>[0-9a-fA-F]+)"
locre = r":(?P<loc>\w+)"

symcase1 = re.compile("^"+regre+"$") # Matches %ret and similar
symcase2 = re.compile("^"+regre+r"\+"+litre+"$") # Matches %ret+$123A2 and similar
symcase3 = re.compile("^"+litre+"$") # Matches $123A2 and similar
symcase4 = re.compile("^"+locre+"$") # Matches :Init and similar

# Parse a symbol (argument after an instruction) and return a dictonary of its
# its parsed results.
def parseSymbol(sym):
    info = {
        "reg": registers["null"],
        "d": 0,
        "dstruct": "I",
        "loc": "",
        "type": ""
    }
    sym = sym.strip()
    reg = "null"
    dtype = ""
    d = ""
    loc = ""
    m = memcase.match(sym)
    if m:
        sym = m.groups()[0]
        info["type"] = "mem"

    m = symcase1.match(sym)
    if m: # If it looks like %ret and similar
        reg = m.group("reg")

    m = symcase2.match(sym)
    if m: # If it looks like %ret+$123A2 and similar
        reg = m.group("reg")
        dtype = m.group("dtype")
        d = m.group("d")

    m = symcase3.match(sym)
    if m: # If it looks like $123A2 and similar
        dtype = m.group("dtype")
        d = m.group("d")

    m = symcase4.match(sym)
    if m: # If it looks like :Init and similar
        info["loc"] = m.group("loc")

    info["reg"] = registers[reg]
    negative = False
    if d != "":
        if "-" in dtype:
            negative = True
            info["dstruct"] = "i"
            dtype = dtype.replace("-","")

        if dtype == "$":
            info["d"] = int(d)
        else:
            info["d"] = int(d, 16)

    return info

def expectRegister(sym):
    r = 0
    if sym[0] == "%":
        r = registers[sym[1:]]
    elif sym[0] == "$":
        r = int(syms[1])
    elif sym.strip() == "":
        r = 0 # null register
    else:
        print("Unknown symbol: %s"%sym)
        sys.exit()

    return r

if __name__ == "__main__":
    outpath = ""
    if len(sys.argv) < 2:
        print("Must provide an input path.")
        sys.exit()

    if len(sys.argv) >= 3:
        outpath = sys.argv[2]
    else:
        outpath = os.path.splitext(sys.argv[1])[0]+".o"

    objectcode = bytes()
    locations = {}
    lookbacks = {}
    with open(sys.argv[1],'r') as f:
        nextlocation = None
        for linenum, line in enumerate(f):
            #Basic commenting
            if line.find(";") != -1:
                line = line[:line.find(";")]

            line = line.strip()
            if line == "":
                continue

            syms = line.split(" ",1)
            args = []
            if len(syms) > 1:
                args = syms[1].split(",")

            if syms[0][-1] == ":":
                nextlocation = syms[0][:-1]
                continue
            else:
                if nextlocation != None:
                    locations[nextlocation] = len(objectcode)
                    nextlocation = None

                ins = syms[0]
                ins_type = instruction_types[ins]
                ins_id = 0
                ins_id = icodes[ins_type] << 4
                if ins_type != "move":
                    ins_id |= ifuns[ins_type].index(ins)

                if ins_type == "control":
                    objectcode += struct.pack("=B", ins_id)
                elif ins_type == "stack":
                    rA = parseSymbol(args[0])['reg']
                    objectcode += struct.pack("=BB", ins_id, rA<<4)
                elif ins_type == "move":  #TODO: Better arg parsing so we can do rm and mr moves
                    infoA = parseSymbol(args[0])
                    infoB = parseSymbol(args[1])
                    d = 0
                    dstruct = "I"
                    rA = infoA["reg"]
                    rB = infoB["reg"]
                    movtype = "rrmov"
                    if infoA["type"] == "mem" and infoB["type"] != "mem":
                        movtype = "mrmov"
                        d = infoA["d"]
                        dstruct = infoA["dstruct"]
                    elif infoA["type"] != "mem" and infoB["type"] == "mem":
                        movtype = "rmmov"
                        d = infoB["d"]
                        dstruct = infoB["dstruct"]
                    elif rA == registers["null"] and infoB["type"] != "mem":
                        movtype = "irmov"
                        d = infoA["d"]
                        dstruct = infoA["dstruct"]
                    elif rA != registers["null"] and rB != registers["null"] and infoB["type"] != "mem":
                        movtype = "rrmov"
                        d = infoA["d"]
                        dstruct = infoA["dstruct"]
                    else:
                        raise AssembleError(linenum,"Invalid mov statement.")

                    # print("Move: %s %X %X %d"%(movtype,rA,rB,d))

                    ins_id |= ifuns[ins_type].index(movtype)
                    objectcode += struct.pack("=BB%s"%dstruct, ins_id, rA<<4|rB,d)

                elif ins_type == "arith":
                    infoA = parseSymbol(args[0])
                    infoB = parseSymbol(args[1])
                    d = infoA['d']
                    dstruct = infoA['dstruct']
                    rA = infoA['reg']
                    rB = infoB['reg']
                    objectcode += struct.pack("=BB%s"%dstruct, ins_id, rA<<4|rB,d)

                elif ins_type == "branch":
                    d = 0
                    dstruct = "I"
                    if len(args) > 0:
                        info = parseSymbol(args[0])
                        d = info["d"]
                        dstruct = info["dstruct"]
                        if info["loc"]:
                            if info["loc"] in locations:
                                d = locations[info["loc"]]
                            else:
                                # Add this location to our lookbacks
                                lookbacks[len(objectcode)] = info["loc"]

                    objectcode += struct.pack("=B%s"%dstruct, ins_id, d)

                elif ins_type == "conditional":
                    rA = parseSymbol(args[0])['reg']
                    rB = parseSymbol(args[1])['reg']
                    objectcode += struct.pack("=BB", ins_id, (rA<<4)|rB)

    for lb in lookbacks:
        #Extract our object code on either side of our lookback.
        o1 = objectcode[:lb+1]
        o2 = objectcode[lb+5:]
        loc = lookbacks[lb]
        if not loc in locations:
            raise(AssembleError(linenum,"Invalid location: %s"%lookbacks[lb]))

        objectcode = o1 + struct.pack("=I", locations[loc]) + o2

    with open(outpath,'wb') as f:
        f.write(objectcode)
