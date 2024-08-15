import json

with open('opcodes.json', 'r') as f:
    opcodes = json.load(f)

unpref = []
cbpref = []

for dct in opcodes['Unprefixed']:
    unpref.append(dct['Name'])

for dct in opcodes['CBPrefixed']:
    cbpref.append(dct['Name'])

prologue = """
enum
{
    ARG_NONE,
    ARG_U8,
    ARG_I8,
    ARG_U16
};

struct OpcodeInfo
{
    std::string fmt_str;
    int arg_type;
};


"""

print(prologue)

print('const OpcodeInfo opcode_info[256] = {')
for x in unpref:
    argt = None
    if 'u8' in x:
        x = x.replace('u8', '{0:02X}')
        argt = 'ARG_U8'
    elif 'i8' in x:
        x = x.replace('i8', '{0:02X}')
        argt = 'ARG_I8'
    elif 'u16' in x:
        x = x.replace('u16', '{0:04X}')
        argt = 'ARG_U16'
    else:
        argt = 'ARG_NONE'
    print(f'    \u007b\"{x}\", {argt}\u007d,')
print('};\n')

print('const OpcodeInfo cb_opcode_info[256] = {')
for x in cbpref:
    print(f'    \u007b\"{x}\", ARG_NONE\u007d,')
print('};\n')
