#include "protocolfield.h"
#include "protocolparser.h"
#include "shuntingyard.h"
#include <QString>
#include <QDomElement>
#include <iostream>
#include <math.h>

TypeData::TypeData() :
    isStruct(false),
    isSigned(false),
    isBitfield(false),
    isFloat(false),
    isEnum(false),
    isString(false),
    isFixedString(false),
    isNull(false),
    bits(8)
{
}


TypeData::TypeData(const TypeData& that) :
    isStruct(that.isStruct),
    isSigned(that.isSigned),
    isBitfield(that.isBitfield),
    isFloat(that.isFloat),
    isEnum(that.isEnum),
    isString(that.isString),
    isFixedString(that.isFixedString),
    isNull(that.isNull),
    bits(that.bits)
{
}


/*!
 * Pull a positive integer value from a string
 * \param string is the source string, which can contain a decimal or hexadecimal (0x) value
 * \param ok receives false if there was a problem with the decode
 * \return the positive integer value or 0 if there was a problem
 */
int TypeData::extractPositiveInt(const QString& string, bool* ok)
{
    QString number = string;

    if(number.contains("0x", Qt::CaseInsensitive))
    {
        QRegExp rx("[^0123456789AaBbCcDdEeFf]");
        number.replace(rx, "");
        return number.toInt(ok, 16);
    }
    else
    {
        QRegExp rx("[^0123456789]");
        number.replace(rx, "");
        return number.toInt(ok, 10);
    }

}


/*!
 * Pull a double precision value from a string
 * \param string is the source string
 * \param ok receives false if there was a problem with the decode
 * \return the double precision value or 0 if there was a problem
 */
double TypeData::extractDouble(const QString& string, bool* ok)
{
    QString number = string;

    if(number.contains("0x", Qt::CaseInsensitive) && !number.contains("."))
    {
        QRegExp rx("[^0123456789AaBbCcDdEeFf]");
        number.replace(rx, "");
        return (double)number.toInt(ok, 16);
    }
    else
    {
        QRegExp rx("[^0123456789-\\.]");
        number.replace(rx, "");
        return number.toDouble(ok);
    }
}


/*!
 * Extract the type information from the type string, for in memory types
 * \param type is the type string
 * \param support gives the protocol support options
 * \param name is the name of this field, used for warnings
 * \param inMemory is true if this is an in-memory type string, else encoded
 */
void TypeData::extractType(const QString& typeString, const ProtocolSupport& support, const QString& name, bool inMemory)
{
    QString type(typeString);

    // Reset to defaults first
    *this = TypeData();

    if(type.startsWith("n", Qt::CaseInsensitive))
        isNull = true;
    else if(type.startsWith("stru", Qt::CaseInsensitive))
    {
        if(inMemory)
            isStruct = true;
        else
            return;
    }
    else if(type.startsWith("string", Qt::CaseInsensitive))
    {
        isString = true;
        isFixedString = false;

        bits = 8;
    }
    else if(type.startsWith("fixedstring", Qt::CaseInsensitive))
    {
        isString = true;
        isFixedString = true;

        bits = 8;
    }
    else if(type.startsWith("b", Qt::CaseInsensitive))
    {
        // Get the number of bits, between 1 and 32 inclusive
        bits = extractPositiveInt(type);

        if(support.bitfield == false)
        {
            std::cout << name.toStdString() << ": " << "bitfield support is disabled in this protocol" << std::endl;

            // if bits is 1, then it becomes 8. If it is 8 then it
            // becomes 8, if its 9 it becomes 16, etc.
            bits = 8*((bits + 7)/8);
        }
        else
        {
            // Bitfields cannot be floats and cannot be signed
            isBitfield = true;

            // Bitfields must have at least one bit, and less than 33 bits
            if(bits < 1)
            {
                bits = 1;
                std::cout << name.toStdString() << ": " << "bitfields must have a bit width of at least one" << std::endl;
            }
            else if(bits > 32)
            {
                std::cout << name.toStdString() << ": " <<  "bitfields must have a bit width of 32 or less" << std::endl;
                bits = 32;
            }
        }
    }
    else if(type.startsWith("e", Qt::CaseInsensitive))
    {
        // enumeration types are only for in-memory, never encoded
        if(inMemory)
        {
            isEnum = true;
        }
        else
        {
            isEnum = false;
        }

        bits = 8;
    }
    else
    {
        bits = extractPositiveInt(type);

        if(type.startsWith("u", Qt::CaseInsensitive))
        {
            isSigned = false;
        }
        else
        {
            isSigned = true;

            if(type.startsWith("f", Qt::CaseInsensitive))
            {
                isFloat = true;

                // "float" is not a warning
                if(bits == 0)
                    bits = 32;
            }
            else if(type.startsWith("d", Qt::CaseInsensitive))
            {
                isFloat = true;

                // "double" is not a warning
                if(bits == 0)
                    bits = 64;
            }
            else if(!type.startsWith("s", Qt::CaseInsensitive) && !type.startsWith("i", Qt::CaseInsensitive))
            {
                std::cout << name.toStdString() << ": " << "in memory type name not understood, signed integer assumed" << std::endl;
            }
        }

        if(isFloat)
        {
            if(inMemory)
            {
                if((bits != 32) && (bits != 64))
                {
                    std::cout << name.toStdString() << ": " << "in memory float types must be 32 or 64 bits" << std::endl;

                    if(bits < 32)
                        bits = 32;
                    else
                        bits = 64;
                }
            }
            else if((bits != 16) && (bits != 24) && (bits != 24) && (bits != 64))
            {
                std::cout << name.toStdString() << ": " << "encoded float types must be 16, 24, 32, or 64 bits" << std::endl;

                if(bits < 16)
                    bits = 16;
                else if(bits < 24)
                    bits = 24;
                else if(bits < 32)
                    bits = 32;
                else
                    bits = 64;

                if((bits < 32) && (support.specialFloat == false))
                {
                    std::cout << name.toStdString() << ": " << "non-standard float bit widths are disabled in this protocol" << std::endl;
                    bits = 32;
                }
            }

            if((bits > 32) && (support.float64 == false))
            {
                std::cout << name.toStdString() << ": " << "64 bit float support is disabled in this protocol" << std::endl;
                bits = 32;
            }

        }// if float
        else
        {
            if(inMemory)
            {
                if((bits != 8) && (bits != 16) && (bits != 32) && (bits != 64))
                {
                    std::cout << name.toStdString() << ": " << "in memory integer types must be 8, 16, 32, or 64 bits" << std::endl;

                    if(bits > 32)
                        bits = 64;
                    else if(bits > 16)
                        bits = 32;
                    else if(bits > 8)
                        bits = 16;
                    else
                        bits = 8;
                }
            }
            else if(((bits % 8) != 0) || (bits > 64))
            {
                std::cout << name.toStdString() << ": " << "encoded integer types must be 8, 16, 24, 32, 40, 48, 56, or 64 bits" << std::endl;

                if(bits > 56)
                    bits = 64;
                else if(bits > 48)
                    bits = 56;
                else if(bits > 40)
                    bits = 48;
                else if(bits > 32)
                    bits = 40;
                else if(bits > 24)
                    bits = 32;
                else if(bits > 16)
                    bits = 24;
                else if(bits > 8)
                    bits = 16;
                else
                    bits = 8;
            }

            if((bits > 32) && (support.int64 == false))
            {
                std::cout << name.toStdString() << ": " << "Integers greater than 32 bits are disabled in this protocol" << std::endl;
                bits = 32;
            }

        }// if integer

    }// else if float or integer

}// TypeData::extractType


QString TypeData::toTypeString(QString enumName, QString structName) const
{
    QString typeName;

    if(isString)
        typeName = "char";
    else if(isBitfield)
        typeName = "uint32_t";
    else if(isEnum)
    {
        typeName = enumName;
    }
    else if(isStruct)
    {
        typeName = structName;

        // Make user it ends with "_t";
        if(!typeName.contains("_t"))
            typeName += "_t";
    }
    else
    {
        // create the type name
        if(isFloat)
        {
            if(bits > 32)
                typeName = "double";
            else
                typeName = "float";

        }// if floating point type
        else
        {
            if(isSigned)
                typeName = "int";
            else
                typeName = "uint";

            // Add the bits, we use only valid native type widths
            if(bits > 32)
                typeName += "64_t";
            else if(bits > 16)
                typeName += "32_t";
            else if(bits > 8)
                typeName += "16_t";
            else
                typeName += "8_t";

        }// else if integer type
    }

    return typeName;

}// toTypeString


/*!
 * Construct a blank protocol field
 * \param protocolName is the name of the protocol
 * \param prtoocolPrefix is the naming prefix
 * \param supported indicates what the protocol can support
 */
ProtocolField::ProtocolField(const QString& protocolName, const QString& protocolPrefix, ProtocolSupport supported):
    Encodable(protocolName, protocolPrefix, supported),
    encodedMin(0),
    encodedMax(0),
    scaler(1)
{

}


/*!
 * Construct a protocol field by parsing the DOM
 * \param protocolName is the name of the protocol
 * \param prtoocolPrefix is the naming prefix
 * \param supported indicates what the protocol can support
 * \param field is the DOM element
 */
ProtocolField::ProtocolField(const QString& protocolName, const QString& protocolPrefix, ProtocolSupport supported, const QDomElement& field):
    Encodable(protocolName, protocolPrefix, supported),
    encodedMin(0),
    encodedMax(0),
    scaler(1)
{
    parse(field);
}


/*!
 * Reset all data to defaults
 */
void ProtocolField::clear(void)
{
    Encodable::clear();

    enumName.clear();
    encodedMin = encodedMax = 0;
    scaler = 1;
    defaultValue.clear();
    constantValue.clear();
    encodedType = inMemoryType = TypeData();

}// ProtocolField::clear


/*!
 * Parse the DOM to determine the details of this ProtocolField
 * \param field is the DOM element
 */
void ProtocolField::parse(const QDomElement& field)
{
    clear();

    name = field.attribute("name").trimmed();
    QString memoryTypeString = field.attribute("inMemoryType").trimmed();

    if(name.isEmpty())
    {
        name = "notprovided";
        std::cout << "data tag without a name: " << field.text().toStdString() << std::endl;
    }

    // maybe its an enum or a external struct?
    if(memoryTypeString.isEmpty())
    {
        // Maybe its an enum?
        if(!field.attribute("enum").isEmpty())
            memoryTypeString = "enum";
        else if(!field.attribute("struct").isEmpty())
            memoryTypeString = "struct";
        else
        {
            memoryTypeString = "null";
            std::cout << "failed to find inMemoryType attribute for: " << name.toStdString() << " \"null\" assumed." << std::endl;
        }
    }

    // Extract the in memory type
    inMemoryType.extractType(memoryTypeString, support, name, true);

    // The encoded type string, this can be empty which implies encoded is same as memory
    QString encodedTypeString = field.attribute("encodedType").trimmed();
    if(encodedTypeString.isEmpty())
    {
        encodedType = inMemoryType;

        // Encoded types are never enums
        if(encodedType.isEnum)
            encodedType.isEnum = false;
    }
    else
        encodedType.extractType(encodedTypeString, support, name, false);

    // Pull all the attribute data
    QString maxString = field.attribute("max").trimmed();
    QString minString = field.attribute("min").trimmed();
    QString scalerString = field.attribute("scaler").trimmed();
    QString structName = field.attribute("struct").trimmed();
    array = field.attribute("array").trimmed();
    variableArray = field.attribute("variableArray").trimmed();
    dependsOn = field.attribute("dependsOn").trimmed();
    enumName = field.attribute("enum").trimmed();
    defaultValue = field.attribute("default").trimmed();
    constantValue = field.attribute("constant").trimmed();
    comment = ProtocolParser::getComment(field);

    if(inMemoryType.isNull)
    {
        // Null types are not in memory, and cannot have defaults or variable arrays
        notInMemory = true;
        variableArray.clear();
        defaultValue.clear();

        // A special case, where we use the encoded type data in place of the
        // in memory type. This handles cases where (for example) we want to
        // encode a constant bitfield, or a string, but have no data in memory
        if(!encodedType.isNull)
        {
            inMemoryType = encodedType;
            inMemoryType.isNull = true;
        }
        else
        {
            std::cout << "both in-memory and encoded types for: " << name.toStdString() << " are \"null\", nothing to do." << std::endl;
            return;
        }
    }

    if(inMemoryType.isEnum && enumName.isEmpty())
    {
        std::cout << name.toStdString() << ": " << "enumeration name is missing, type changed to unsigned" << std::endl;
        inMemoryType.isEnum = encodedType.isEnum = false;
    }

    if(inMemoryType.isStruct)
    {
        if(!encodedType.isStruct)
            std::cout << name.toStdString() << ": " << "encoded type information ignored for external struct" << std::endl;

        if(structName.isEmpty())
        {
            std::cout << name.toStdString() << ": " << "struct name is missing, struct name \"unknown\" used, probable compile failure" << std::endl;
            structName = "unknown";
        }
    }

    if(encodedType.bits > inMemoryType.bits)
    {
        std::cout << name.toStdString() << ": " << "Encoded type cannot use more bits than in-memory" << std::endl;
        encodedType.bits = inMemoryType.bits;
    }

    if(inMemoryType.isBitfield)
    {
        if(!encodedTypeString.isEmpty() && !encodedType.isBitfield)
            std::cout << name.toStdString() << ": encoded type ignored because in memory type is bitfield" << std::endl;

        // make the encoded type follow the in memory type for bit fields
        encodedType.isBitfield = true;
        encodedType.bits = inMemoryType.bits;
    }

    // It is possible for the in memory type to not be a bit field, but the
    // encoding could be. The most common case for this would be an in-memory
    // enumeration in which the maximum enumeration fits in fewer than 8 bits
    if(encodedType.isBitfield)
    {
        if(!dependsOn.isEmpty())
        {
            std::cout << name.toStdString() << ": bitfields cannot use dependsOn" << std::endl;
            dependsOn.clear();
        }

        if(!array.isEmpty())
        {
            std::cout << name.toStdString() << ": bitfields encodings cannot use arrays" << std::endl;
            array.clear();
            variableArray.clear();
        }

        if(!maxString.isEmpty() || !minString.isEmpty() || !scalerString.isEmpty())
        {
            std::cout << name.toStdString() << ": min, max, and scaler are ignored because encoded type or in memory type is bitfield" << std::endl;
            maxString.clear();
            minString.clear();
            scalerString.clear();
        }
    }

    // if either type says string, than they both are string
    if(inMemoryType.isString != encodedType.isString)
    {
        inMemoryType.isString = encodedType.isString = true;
        inMemoryType.bits = encodedType.bits = 8;
    }

    // if either type says fixed string, than they both are fixed string
    if(inMemoryType.isFixedString != encodedType.isFixedString)
    {
        inMemoryType.isString = encodedType.isString = true;
        inMemoryType.isFixedString = encodedType.isFixedString = true;
        inMemoryType.bits = encodedType.bits = 8;
    }

    if(array.isEmpty() && !variableArray.isEmpty())
    {
        std::cout << name.toStdString() << ": Must specify array length to specify variable array length" << std::endl;
        variableArray.clear();
    }

    if(!dependsOn.isEmpty() && !variableArray.isEmpty())
    {
        std::cout << name.toStdString() << ": variable length arrays cannot also use dependsOn" << std::endl;
        dependsOn.clear();
    }

    if(!scalerString.isEmpty() && !maxString.isEmpty())
    {
        std::cout << name.toStdString() << ": scaler ignored because max is provided" << std::endl;
        scalerString.clear();
    }

    if(!maxString.isEmpty() || !minString.isEmpty() || !scalerString.isEmpty())
    {
        if(inMemoryType.isStruct || inMemoryType.isString || inMemoryType.isBitfield || encodedType.isBitfield || encodedType.isNull)
        {
            std::cout << name.toStdString() << ": min, max, and scaler do not apply to this type data" << std::endl;
            maxString.clear();
            minString.clear();
            scalerString.clear();
        }
        else if(encodedType.isFloat)
        {
            std::cout << name.toStdString() << ": min, max, and scaler are ignored because encoded type is float, which are never scaled" << std::endl;
            maxString.clear();
            minString.clear();
            scalerString.clear();
        }
    }

    if(inMemoryType.isString)
    {
        // Strings have to be arrays, default to 64 characters
        if(array.isEmpty())
        {
            std::cout << name.toStdString() << ": string length not provided, assuming 64" << std::endl;
            array = "64";
        }

        // Strings are always variable length, through null termination
        if(!variableArray.isEmpty())
        {
            std::cout << name.toStdString() << ": strings cannot use variableAray attribute, they are always variable length through null termination (unless fixedstring)" << std::endl;
            variableArray.clear();
        }

        if(!dependsOn.isEmpty())
        {
            std::cout << name.toStdString() << ": strings cannot use dependsOn" << std::endl;
            dependsOn.clear();
        }

    }// if string

    if(encodedType.isNull)
    {
        if(!constantValue.isEmpty())
        {
            std::cout << name.toStdString() << ": constant value does not make sense for types that are not encoded (null)" << std::endl;
            variableArray.clear();
        }

        if(!variableArray.isEmpty())
        {
            std::cout << name.toStdString() << ": variable length arrays do not make sense for types that are not encoded (null)" << std::endl;
            variableArray.clear();
        }

        if(!dependsOn.isEmpty())
        {
            std::cout << name.toStdString() << ": dependsOn does not make sense for types that are not encoded (null)" << std::endl;
            dependsOn.clear();
        }
    }

    bool ok;

    if(!minString.isEmpty())
    {
        if(encodedType.isSigned)
            std::cout << name.toStdString() << ": min value ignored because encoded type is signed" << std::endl;
        else
        {
            encodedMin = ShuntingYard::computeInfix(minString, &ok);

            if(!ok)
                std::cout << name.toStdString() + ": min is not a number, 0.0 assumed" << std::endl;
        }
    }

    if(!maxString.isEmpty())
    {
        encodedMax = ShuntingYard::computeInfix(maxString, &ok);
        if(!ok)
        {
            std::cout << name.toStdString() + ": max is not a number, 1.0 assumed" << std::endl;
            encodedMax = 1.0;
        }

        if(encodedType.isSigned)
        {
            scaler = (powf(2.0, encodedType.bits-1) - 1.0)/encodedMax;

            // This is not exactly true, there is one more bit that could be used,
            // but this makes conciser commenting, and is clearer to the user
            encodedMin = -encodedMax;
        }
        else
        {
            scaler = (powf(2.0, encodedType.bits) - 1.0)/(encodedMax - encodedMin);
        }

    }
    else if(!scalerString.isEmpty())
    {
        scaler = ShuntingYard::computeInfix(scalerString, &ok);

        if(!ok)
        {
            std::cout << name.toStdString() + ": scaler is not a number, 1.0 assumed" << std::endl;
            scaler = 1.0;
        }
        else if(scaler <= 0.0)
        {
            std::cout << name.toStdString() + ": scaler must be greater than zero, 1.0 used" << std::endl;
            scaler = 1.0;
        }

        if(encodedType.isSigned)
        {
            encodedMax = (powf(2.0, encodedType.bits-1) - 1.0)/scaler;

            // This is not exactly true, there is one more bit that could be used,
            // but this makes conciser commenting, and is clearer to the user
            encodedMin = -encodedMax;
        }
        else
        {
            encodedMax = encodedMin + (powf(2.0, encodedType.bits) - 1.0)/scaler;
        }

    }

    // Max must be larger than minimum
    if(encodedMin > encodedMax)
    {
        encodedMin = encodedMax = 0.0;
        scaler = 1.0;
        std::cout << name.toStdString() << ": max is not more than min, encoding not scaled" << std::endl;
    }

    // Just the type data
    typeName = inMemoryType.toTypeString(enumName, prefix + structName);

    if(!constantValue.isEmpty())
    {
        if(inMemoryType.isStruct)
        {
            std::cout << name.toStdString() << ": structure cannot have a constant value" << std::endl;
            constantValue.clear();
        }
        else if(!defaultValue.isEmpty())
        {
            std::cout << name.toStdString() << ": fields with default values cannot also be constant" << std::endl;
            constantValue.clear();
        }
    }

    // Check for data not encoded
    if(encodedType.isNull)
        notEncoded = true;

    // Check for data that does not appear in-memory
    if(inMemoryType.isNull)
        notInMemory = true;

    // Check for data that is encoded constant
    if(!constantValue.isEmpty())
        constant = true;

}// ProtocolField::parse


/*!
 * Get the declaration for this field as a member of a structure
 * \return the declaration string
 */
QString ProtocolField::getDeclaration(void) const
{
    QString output;

    if(notInMemory)
        return output;

    output = "    " + typeName + " " + name;

    if(usesBitfields())
        output += " : " + QString().setNum(inMemoryType.bits);
    else if(isArray())
        output += "[" + array + "]";

    output += ";";

    if(comment.isEmpty())
    {
        if(!constantValue.isEmpty())
            output += " //!< Field is constant.";
    }
    else
    {
        output += " //!< " + comment;
        if(!constantValue.isEmpty())
            output += ". Field is constant.";
    }

    output += "\n";

    return output;

}// ProtocolField::getDeclaration


/*!
 * Return the inclue directive needed for this encodable. Mostly this is empty,
 * but for external structures we need to bring in the include file
 */
QString ProtocolField::getIncludeDirective(void)
{
    if(inMemoryType.isStruct)
    {
        QString output = ProtocolParser::lookUpIncludeName(typeName);

        if(output.isEmpty())
        {
            if(!notEncoded)
            {
                // In this case, we guess at the include name
                output = typeName;
                output.remove("_t");
                output += ".h";
                std::cout << name.toStdString() + ": unknown include for " << typeName.toStdString() << "; guess supplied" << std::endl;
            }
            else
            {
                // If there is no encoding for this field, then we assume its
                // some externally supplied struct definition, and its up to the
                // user to specify the correct include file, since we don't need
                // the encoding functions anyway.
                return QString();
            }
        }

        return output;
    }
    else
        return QString();
}


/*!
 * Return the signature of this field in an encode function signature
 * \return The encode signature of this field
 */
QString ProtocolField::getEncodeSignature(void) const
{
    if(notEncoded || notInMemory || !constantValue.isEmpty())
        return "";
    else if(isArray())
        return ", const " + typeName + " " + name + "[" + array + "]";
    else if(!inMemoryType.isStruct)
        return ", " + typeName + " " + name;
    else
        return ", const " + typeName + "* " + name;
}


/*!
 * Return markdown formatted information about this encodables fields. This
 * documents data on the wire. Data encoded null are not documented.
 * \param indent is the spaces to start the line with.
 * \return The markdown text for this field
 */
QString ProtocolField::getMarkdown(QString indent) const
{
    QString output = indent;

    if(encodedType.isNull)
        return QString();
    else if(inMemoryType.isNull)
    {
        output += "* reserved space in the packet. ";
        if(isArray())
            output += "[`" + array + "`]*";

        output += QString().setNum(encodedType.bits) + " bits.";

        if(!constantValue.isEmpty())
            output += " data are given constant value `" + constantValue + "`.";

    }
    else if(inMemoryType.isStruct)
    {
        // Backtick quotes to make the name be "code"
        output += "* `" + name + "` :  ";

        if(!comment.isEmpty())
            output += comment + ".";

        if(isArray())
        {
            if(variableArray.isEmpty())
                output += "  Repeat `" + array + "` times.";
            else
                output += "  Repeat `" + variableArray + "` times, up to `" + array + "` times.";
        }

        if(!dependsOn.isEmpty())
            output += "  Only included if `" + dependsOn + "` is non-zero.";

        output += "\n";
        output += ProtocolParser::getStructureSubMarkdown(typeName, indent + "    ");
        return output;
    }
    else
    {
        // Backtick quotes to make the name be "code"
        output += "* `" + name + "` :  ";

        if(!comment.isEmpty())
            output += comment + ".  ";

        if(encodedType.isString)
        {
            if(encodedType.isFixedString)
                output += "Zero terminated fixed length string of characters with length of " + array + ".";
            else
                output += "Zero terminated variable length string of characters with a maximum length of " + array + ".";
        }
        else if(encodedType.isBitfield)
            output += QString().setNum(encodedType.bits) + " bit bitfield.";
        else
        {
            if(isArray())
            {
                if(variableArray.isEmpty())
                    output += "[`" + array + "`]*";
                else
                    output += "variable length array: [`min(" + variableArray + ", " + array + ")`]*";
            }

            if(encodedType.isFloat)
                output += QString().setNum(encodedType.bits) + " bit floating point.";
            else if(encodedType.isEnum)
                output += QString().setNum(encodedType.bits) + " bit enumeration.";
            else if(encodedType.isSigned)
            {
                output += QString().setNum(encodedType.bits) + " bit signed integer.";

                if(encodedMax != 0.0)
                    output += "  Scaled by " + getNumberString(scaler) + " from " + getNumberString(encodedMin) + " to " + getNumberString(encodedMax) + ".";
            }
            else
            {
                output += QString().setNum(encodedType.bits) + " bit unsigned integer.";

                if(encodedMax != 0.0)
                    output += "  Scaled by " + getNumberString(scaler) + " from " + getNumberString(encodedMin) + " to " + getNumberString(encodedMax) + ".";
            }
        }

        if(!constantValue.isEmpty())
            output += "  Always has the constant value of `" + constantValue + "` .";

        if(!dependsOn.isEmpty())
            output += "  Only included if `" + dependsOn + "` is non-zero.";
    }
    return output + "\n";
}


/*!
 * Get the next lines(s) of source coded needed to encode this field
 * \param isBigEndian should be true for big endian encoding.
 * \param encLength is appended for length information of this field.
 * \param bitcount points to the running count of bits in a bitfields and should persist between calls
 * \param isStructureMember should be true if the left hand side is a
 *        member of a user structure, else the left hand side is a pointer
 *        to the inMemoryType
 * \return The string to add to the source file that encodes this field.
 */
QString ProtocolField::getEncodeString(bool isBigEndian, EncodedLength& encLength, int* bitcount, bool isStructureMember) const
{
    if(encodedType.isBitfield)
    {
        return getEncodeStringForBitfield(bitcount, isStructureMember);
    }
    else
    {
        QString output;

        // If previous output was a bit field, then we need to close it out
        if(*bitcount != 0)
            output = getCloseBitfieldString(bitcount, &encLength);

        if(inMemoryType.isString)
            output += getEncodeStringForString(encLength, isStructureMember);
        else if(inMemoryType.isStruct)
            output += getEncodeStringForStructure(encLength, isStructureMember);
        else
            output += getEncodeStringForField(isBigEndian, encLength, isStructureMember);

        return output;
    }
}


/*!
 * Get the next lines(s) of source coded needed to decode this field
 * \param isBigEndian should be true for big endian encoding.
 * \param bitcount points to the running count of bits in a bitfields and should persist between calls
 * \param isStructureMember should be true if the left hand side is a
 *        member of a user structure, else the left hand side is a pointer
 *        to the inMemoryType
 * \param defaultEnabled should be true to handle defaults
 * \return The string to add to the source file that encodes this field.
 */
QString ProtocolField::getDecodeString(bool isBigEndian, int* bitcount, bool isStructureMember, bool defaultEnabled) const
{
    if(encodedType.isBitfield)
    {
        return getDecodeStringForBitfield(bitcount, isStructureMember);
    }
    else
    {
        QString output;

        // If previous output was a bit field, then we need to close it out
        if(*bitcount != 0)
            output = getCloseBitfieldString(bitcount);

        if(inMemoryType.isString)
            output += getDecodeStringForString(isStructureMember);
        else if(inMemoryType.isStruct)
            output += getDecodeStringForStructure(isStructureMember);
        else
            output += getDecodeStringForField(isBigEndian, isStructureMember, defaultEnabled);

        return output;
    }
}


/*!
 * Return the string that sets this encodable to its default value in code
 * \param isStructureMember should be true if this field is accessed through a "user" structure pointer
 * \return the string to add to the source file, including line feed
 */
QString ProtocolField::getSetToDefaultsString(bool isStructureMember) const
{
    QString output;
    QString access;

    if(defaultValue.isEmpty())
        return output;

    // Write out the defaults code
    if(isArray())
    {
        if(isStructureMember)
            access = "user->";

        output += "    for(i = 0; i < " + array + "; i++)\n";
        output += "        " + access + name + "[i] = " + defaultValue + ";\n";
    }
    else
    {
        if(isStructureMember)
            access = "user->";
        else
            access = "*";

        // Direct pointer access
        output += "    " + access + name + " = " + defaultValue + ";\n";
    }

    return output;

}// ProtocolField::getSetToDefaultsString


/*!
 * Get the next lines(s) of source coded needed to encode this bitfield field
 * \param bitcount points to the running count of bits in this string of
 *        bitfields, and will be updated by this fields encoded bit count.
 * \param isStructureMember should be true if the left hand side is a
 *        member of a user structure, else the left hand side is a pointer
 *        to the inMemoryType
 * \return The string to add to the source file that encodes this field.
 */
QString ProtocolField::getEncodeStringForBitfield(int* bitcount, bool isStructureMember) const
{
    QString output;

    if(encodedType.isNull)
        return output;

    if(!comment.isEmpty())
        output += "    // " + comment + "\n";

    if(constantValue.isEmpty())
    {
        if(isStructureMember)
            output += "    encodeBitfield((uint32_t)user->" + name;
        else
            output += "    encodeBitfield((uint32_t)" + name;
    }
    else
        output += "    encodeBitfield((uint32_t)" + constantValue;

    output += ", data, &byteindex, &bitcount, " + QString().setNum(encodedType.bits) + ");\n";
    *bitcount += encodedType.bits;

    return output;

}// ProtocolField::getEncodeBitfieldString


/*!
 * Get the next lines(s) of source coded needed to decode this bitfield field
 * \param bitcount points to the running count of bits in this string of
 *        bitfields, and will be updated by this fields encoded bit count.
 * \param isStructureMember should be true if the left hand side is a
 *        member of a user structure, else the left hand side is a pointer
 *        to the inMemoryType
 * \return The string to add to the source file that decodes this field.
 */
QString ProtocolField::getDecodeStringForBitfield(int* bitcount, bool isStructureMember) const
{
    QString output;

    if(encodedType.isNull)
        return output;

    if(!comment.isEmpty())
        output += "    // " + comment + "\n";

    if(inMemoryType.isNull)
    {
        if(comment.isEmpty())
            output += "    // reserved bits\n";

        output += "    decodeBitfield(data, &byteindex, &bitcount, " + QString().setNum(encodedType.bits) + ");\n";
    }
    else
    {
        if(isStructureMember)
            output += "    user->"; // Access via structure pointer
        else
            output += "    *";      // Access via direct pointer

        // we cast here, because the inMemoryType might not be a uint32_t
        output += name + " = (" + typeName + ")decodeBitfield(data, &byteindex, &bitcount, " + QString().setNum(encodedType.bits) + ");\n";
    }

    *bitcount += encodedType.bits;

    return output;

}// ProtocolField::getDecodeStringForBitfield


/*!
 * Get the next lines of source needed to encode this string field
 * \param encLength is appended for length information of this field.
 * \param isStructureMember should be true if the left hand side is a
 *        member of a user structure, else the left hand side is a pointer
 *        to the inMemoryType
 * \return The string to add to the source file to that encodes this field
 */
QString ProtocolField::getEncodeStringForString(EncodedLength& encLength, bool isStructureMember) const
{
    QString output;
    QString lhs;

    if(encodedType.isNull)
        return output;

    if(isStructureMember)
        lhs = "user->";
    else
        lhs = "";

    if(!comment.isEmpty())
        output += "    // " + comment + "\n";

    if(constantValue.isEmpty())
        output += "    stringToBytes(" + lhs + name + ", data, &byteindex, " + array;
    else
    {
        // constantValue is a string literal, so include the quotes. Except for
        // a special case. If constantValue ends in "()" then we assume its a
        // function or macro call
        if(constantValue.contains("(") && constantValue.contains(")"))
            output += "    stringToBytes(" + constantValue + ", data, &byteindex, " + array;
        else
            output += "    stringToBytes(\"" + constantValue + "\", data, &byteindex, " + array;
    }

    if(inMemoryType.isFixedString)
        output += ", 1);\n";
    else
        output += ", 0);\n";

    // Add to the length as a string. Note that a fixed string is not a string at all as far as the length is concerned
    encLength.addToLength(array, !inMemoryType.isFixedString, false, !dependsOn.isEmpty(), !defaultValue.isEmpty());

    return output;

}// ProtocolField::getEncodeStringForString


/*!
 * Get the next lines of source needed to decode this string field
 * \param isStructureMember should be true if the left hand side is a
 *        member of a user structure, else the left hand side is a pointer
 *        to the inMemoryType
 * \return The string to add to the source file to that decodes this field
 */
QString ProtocolField::getDecodeStringForString(bool isStructureMember) const
{
    QString output;

    if(encodedType.isNull)
        return output;

    QString lhs;

    if(isStructureMember)
        lhs = "user->";
    else
        lhs = "";

    if(!comment.isEmpty())
        output += "    // " + comment + "\n";

    output += "    stringFromBytes(" + lhs + name + ", data, &byteindex, " + array;

    if(inMemoryType.isFixedString)
        output += ", 1);\n";
    else
        output += ", 0);\n";

    return output;

}// ProtocolField::getDecodeStringForString


/*!
 * Return the string that is used to encode this structure
 * \param encLength is appended for length information of this field.
 * \param isStructureMember is true if this encodable is accessed by structure pointer
 * \return the string to add to the source to encode this structure
 */
QString ProtocolField::getEncodeStringForStructure(EncodedLength& encLength, bool isStructureMember) const
{
    QString output;
    QString access;
    QString spacing = "    ";

    if(encodedType.isNull)
        return output;

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    if(!dependsOn.isEmpty())
    {
        if(isStructureMember)
            output += spacing + "if(user->" + dependsOn + ")\n";
        else
            output += spacing + "if(" + dependsOn + ")\n";
        output += spacing + "{\n";
        spacing += "    ";
    }

    if(isArray())
    {
        encLength.addToLength("getMinLengthOf" + typeName + "()*" + array, false, !variableArray.isEmpty(), !dependsOn.isEmpty(), !defaultValue.isEmpty());

        if(variableArray.isEmpty())
        {
            output += spacing + "for(i = 0; i < " + array + "; i++)\n";
        }
        else
        {
            if(isStructureMember)
                output += spacing + "for(i = 0; i < (int)user->" + variableArray + " && i < " + array + "; i++)\n";
            else
                output += spacing + "for(i = 0; i < (int)" + variableArray + " && i < " + array + "; i++)\n";
        }

        if(isStructureMember)
            access = "&user->" + name + "[i]";
        else
            access = "&" + name + "[i]";

        output += spacing + "    byteindex = encode" + typeName + "(data, byteindex, " + access + ");\n";
    }
    else
    {
        encLength.addToLength("getMinLengthOf" + typeName + "()", false, false, !dependsOn.isEmpty(), !defaultValue.isEmpty());

        if(isStructureMember)
            access = "&user->" + name;
        else
            access = name;  // in this case, name is already pointer, so we don't need "&"

        output += spacing + "byteindex = encode" + typeName + "(data, byteindex, " + access + ");\n";
    }

    if(!dependsOn.isEmpty())
        output += "    }\n";

    return output;

}// ProtocolField::getEncodeStringForStructure


/*!
 * Get the next lines of source needed to decode this external structure field
 * \param isStructureMember should be true if the left hand side is a
 *        member of a user structure, else the left hand side is a pointer
 *        to the inMemoryType
 * \return The string to add to the source file to that decodes this field
 */
QString ProtocolField::getDecodeStringForStructure(bool isStructureMember) const
{
    QString output;
    QString access;
    QString spacing = "    ";

    if(encodedType.isNull)
        return output;

    if(!comment.isEmpty())
        output += "    // " + comment + "\n";

    if(!dependsOn.isEmpty())
    {
        if(isStructureMember)
            output += spacing + "if(user->" + dependsOn + ")\n";
        else
            output += spacing + "if(" + dependsOn + ")\n";
        output += spacing + "{\n";
        spacing += "    ";
    }

    if(isArray())
    {
        if(variableArray.isEmpty())
            output += spacing + "for(i = 0; i < " + array + "; i++)\n";
        else
        {
            if(isStructureMember)
                output += spacing + "for(i = 0; i < (int)user->" + variableArray + " && i < " + array + "; i++)\n";
            else
                output += spacing + "for(i = 0; i < (int)(*" + variableArray + ") && i < " + array + "; i++)\n";
        }

        if(isStructureMember)
            access = "&user->" + name + "[i]";
        else
            access = "&" + name + "[i]";

        output += spacing + "    byteindex = decode" + typeName + "(data, byteindex, " + access + ");\n";
    }
    else
    {
        if(isStructureMember)
            access = "&user->" + name;
        else
            access = name;  // in this case, name is already pointer, so we don't need "&"

        output += spacing + "byteindex = decode" + typeName + "(data, byteindex, " + access + ");\n";
    }

    if(!dependsOn.isEmpty())
        output += "    }\n";

    return output;

}// ProtocolField::getDecodeStringForStructure


/*!
 * Get the next lines(s) of source coded needed to encode this field, which
 * is not a bitfield or a string
 * \param isBigEndian should be true for big endian encoding.
 * \param encLength is appended for length information of this field.
 * \param isStructureMember should be true if the left hand side is a
 *        member of a user structure, else the left hand side is a pointer
 *        to the inMemoryType
 * \return The string to add to the source file that encodes this field.
 */
QString ProtocolField::getEncodeStringForField(bool isBigEndian, EncodedLength& encLength, bool isStructureMember) const
{
    QString output;
    QString endian;
    QString lengthString;
    QString lhs;
    QString constantValue = this->constantValue;

    if(encodedType.isNull)
        return output;

    if(isStructureMember)
        lhs = "user->";
    else
        lhs = "";

    if(!comment.isEmpty())
        output += "    // " + comment + "\n";

    int length = encodedType.bits / 8;
    lengthString.setNum(length);

    // Remember that we could be encoding an array
    if(!array.isEmpty())
        lengthString += "*" + array;

    encLength.addToLength(lengthString, false, !variableArray.isEmpty(), !dependsOn.isEmpty(), !defaultValue.isEmpty());

    // The endian string, which is empty for 1 byte, since
    // endian only applies to multi-byte fields
    if(length > 1)
    {
        if(isBigEndian)
            endian += "Be";
        else
            endian += "Le";
    }
    else
        endian = "";

    // A null encoding is the same as a constant encoding for our purposes
    // here, the only difference is what number gets put in.
    if(inMemoryType.isNull)
    {
        if(constantValue.isEmpty())
            constantValue = "0";
    }

    QString spacing = "    ";

    if(!dependsOn.isEmpty())
    {
        if(isStructureMember)
            output += spacing + "if(user->" + dependsOn + ")\n";
        else
            output += spacing + "if(" + dependsOn + ")\n";
        output += spacing + "{\n";
        spacing += "    ";
    }

    if(encodedMax > encodedMin)
    {
        // The scaled outputs. Note that scaled outputs never encode
        // in floating point, since floats carry their scaling with them

        // Additional commenting to describe the scaling
        output += spacing + "// Range of " + name + " is " + getNumberString(encodedMin) + " to " + getNumberString(encodedMax) +  ".\n";

        // Handle the array
        if(!array.isEmpty())
        {
            if(variableArray.isEmpty())
                output += spacing + "for(i = 0; i < " + array + "; i++)\n";
            else
                output += spacing + "for(i = 0; i < (int)" + lhs + variableArray + " && i < " + array + "; i++)\n";

            // indent the next line
            output += "    ";
        }

        output += spacing;

        // Handle the in-memory part
        if(inMemoryType.isFloat)
        {
            if(inMemoryType.bits > 32)
                output += "float64";
            else
                output += "float32";
        }
        else if(inMemoryType.isSigned)
            output += "int" + QString().setNum(inMemoryType.bits);
        else
            output += "uint" + QString().setNum(inMemoryType.bits);

        output += "ScaledTo";

        // Now the encoded part:

        // Number of bytes
        output += QString().setNum(length);

        // Signed or unsigned
        if(encodedType.isSigned)
            output += "Signed";
        else
            output += "Unsigned";

        // More of the encode call signature, including endian
        output += endian + "Bytes(";

        if(constantValue.isEmpty())
        {
            // The reference to the data
            output += lhs + name;

            if(!array.isEmpty())
                output += "[i]";
        }
        else
            output += constantValue;

        output += ", data, &byteindex";

        // Signature changes for signed versus unsigned
        if(!encodedType.isSigned)
            output += ", " + getNumberString(encodedMin, inMemoryType.bits);

        output +=  ", " + getNumberString(scaler, inMemoryType.bits);

        output += ");\n";

    }// if scaled
    else if(encodedType.isFloat)
    {
        // In this case we are encoding as a floating point. No scaling
        // here, because floats carry their scaling with them, but we can
        // choose different bit widths. Notice that we have to cast to the
        // input parameter type, since the user might (for example) have
        // the in-memory type as a double, but the encoded as a float
        QString cast = "(" + encodedType.toTypeString() + ")";

        if(array.isEmpty())
        {
            if(constantValue.isEmpty())
                output += spacing + "float" + QString().setNum(encodedType.bits) + "To" + endian + "Bytes(" + cast + lhs + name + ", data, &byteindex);\n";
            else
                output += spacing + "float" + QString().setNum(encodedType.bits) + "To" + endian + "Bytes(" + cast + constantValue + ", data, &byteindex);\n";
        }
        else
        {
            if(variableArray.isEmpty())
                output += spacing + "for(i = 0; i < " + array + "; i++)\n";
            else
                output += spacing + "for(i = 0; i < (int)" + lhs + variableArray + " && i < " + array + "; i++)\n";

            if(constantValue.isEmpty())
                output += spacing + "    float" + QString().setNum(encodedType.bits) + "To" + endian + "Bytes(" + cast + lhs + name + "[i], data, &byteindex);\n";
            else
                output += spacing + "    float" + QString().setNum(encodedType.bits) + "To" + endian + "Bytes(" + cast + constantValue + ", data, &byteindex);\n";
        }
    }
    else
    {
        // Here we are not scaling, and we are not encoding a float. It may
        // be that the encoded type is the same as the in-memory, but in
        // case it is not we add a cast.
        QString cast = "(" + encodedType.toTypeString() + ")";
        QString opener;

        output += spacing;

        if(encodedType.isSigned)
        {
            opener = "int";
        }
        else
        {
            opener = "uint";
        }

        if(array.isEmpty())
        {
            if(constantValue.isEmpty())
                output += opener + QString().setNum(encodedType.bits) + "To" + endian + "Bytes(" + cast + lhs + name + ", data, &byteindex);\n";
            else
                output += opener + QString().setNum(encodedType.bits) + "To" + endian + "Bytes(" + cast + constantValue + ", data, &byteindex);\n";
        }
        else
        {
            if(variableArray.isEmpty())
                output += "for(i = 0; i < " + array + "; i++)\n";
            else
                output += "for(i = 0; i < (int)" + lhs + variableArray + " && i < " + array + "; i++)\n";

            if(constantValue.isEmpty())
                output += spacing + "    " + opener + QString().setNum(encodedType.bits) + "To" + endian + "Bytes(" + cast + lhs + name + "[i], data, &byteindex);\n";
            else
                output += spacing + "    " + opener + QString().setNum(encodedType.bits) + "To" + endian + "Bytes(" + cast + constantValue + ", data, &byteindex);\n";
        }

    }// else not scaled and not float

    if(!dependsOn.isEmpty())
        output += "    }\n";

    return output;

}// ProtocolField::getEncodeStringForField


/*!
 * Get the next lines(s) of source coded needed to decode this field, which
 * is not a bitfield or a string
 * \param isBigEndian should be true if the protocol uses big endian ordering.
 * \param isStructureMember should be true if the left hand side is a
 *        member of a user structure, else the left hand side is a pointer
 *        to the inMemoryType
 * \param defaultEnabled should be true to enable default handling
 * \return The string to add to the source file that decodes this field.
 */
QString ProtocolField::getDecodeStringForField(bool isBigEndian, bool isStructureMember, bool defaultEnabled) const
{
    QString output;
    QString endian;
    QString spacing = "    ";
    QString lhs;

    if(encodedType.isNull)
        return output;

    if(isStructureMember)
        lhs = "user->"; // member of a structure
    else if(array.isEmpty())
        lhs = "*";      // direct pointer
    else
        lhs = "";       // pointer with array de-referencing

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    int length = encodedType.bits / 8;

    // The endian string, which is empty for 1 byte, since
    // endian only applies to multi-byte fields
    if(length > 1)
    {
        if(isBigEndian)
            endian += "Be";
        else
            endian += "Le";
    }
    else
        endian = "";

    QString lengthString;

    // What is the length in bytes of this field, remember that we could be encoding an array
    lengthString.setNum(length);
    if(!array.isEmpty())
        lengthString += "*" + array;

    if(!dependsOn.isEmpty())
    {
        if(isStructureMember)
            output += spacing + "if(user->" + dependsOn + ")\n";
        else
            output += spacing + "if(" + dependsOn + ")\n";
        output += spacing + "{\n";
        spacing += "    ";
    }

    // If this field has a default value
    if(defaultEnabled && !defaultValue.isEmpty())
    {
        output += spacing + "if(byteindex + " + lengthString + " > numBytes)\n";
        output += spacing + "    return 1;\n";
        output += spacing + "else\n";
        output += spacing + "{\n";
        spacing += "    ";
    }

    if(inMemoryType.isNull)
    {
        // Skip over reserved space
        if(!array.isEmpty())
        {
            output += spacing + "for(i = 0; i < " + array + "; i++)\n";
            output += spacing + "    byteindex += " + QString().setNum(length) + ";\n";
        }
        else
            output += spacing + "byteindex += " + lengthString + ";\n";

    }
    else if(encodedMax > encodedMin)
    {
        // Additional commenting to describe the scaling
        output += spacing + "// Range of " + name + " is " + getNumberString(encodedMin) + " to " + getNumberString(encodedMax) +  ".\n";

        // Handle the array
        if(!array.isEmpty())
        {
            if(variableArray.isEmpty())
                output += spacing + "for(i = 0; i < " + array + "; i++)\n";
            else
                output += spacing + "for(i = 0; i < (int)" + lhs + variableArray + " && i < " + array + "; i++)\n";

            // start the next line
            output += spacing + "    " + lhs + name + "[i] = ";
        }
        else
            output += spacing + lhs + name + " = ";

        // Handle the in-memory part
        if(inMemoryType.isFloat)
        {
            if(inMemoryType.bits > 32)
                output += "float64";
            else
                output += "float32";
        }
        else if(inMemoryType.isSigned)
            output += "int" + QString().setNum(inMemoryType.bits);
        else
            output += "uint" + QString().setNum(inMemoryType.bits);

        output += "ScaledFrom";

        // Now the encoded part:

        // Number of bytes
        output += QString().setNum(length);

        // Signed or unsigned
        if(encodedType.isSigned)
            output += "Signed";
        else
            output += "Unsigned";

        output += endian + "Bytes(data, &byteindex";

        // Signature changes for signed versus unsigned
        if(!encodedType.isSigned)
            output += ", " + getNumberString(encodedMin, inMemoryType.bits);

        output +=  ", " + getNumberString(1.0, inMemoryType.bits) + "/" + getNumberString(scaler, inMemoryType.bits);

        output +=  ");\n";

    }// if scaled
    else if(encodedType.isFloat)
    {
        // In this case we are decoding as a floating point. No scaling
        // here, because floats carry their scaling with them, but we can
        // choose different bit widths.
        if(array.isEmpty())
            output += spacing + lhs + name + " = float" + QString().setNum(encodedType.bits) + "From" + endian + "Bytes(data, &byteindex);\n";
        else
        {
            if(variableArray.isEmpty())
                output += spacing + "for(i = 0; i < " + array + "; i++)\n";
            else
            {
                if(isStructureMember)
                    output += spacing + "for(i = 0; i < (int)user->" + variableArray + " && i < " + array + "; i++)\n";
                else
                    output += spacing + "for(i = 0; i < (int)(*" + variableArray + ") && i < " + array + "; i++)\n";
            }

            output += spacing + "    " + lhs + name + "[i] = float" + QString().setNum(encodedType.bits) + "From" + endian + "Bytes(data, &byteindex);\n";
        }
    }
    else
    {
        // Here we are not scaling, and we are not encoding a float. It may
        // be that the encoded type is the same as the in-memory, but in
        // case it is not we add a cast.
        QString cast = "(" + typeName + ")";
        QString opener;

        if(encodedType.isSigned)
            opener = "int";
        else
            opener = "uint";

        if(array.isEmpty())
            output += spacing + lhs + name + " = " + cast + opener + QString().setNum(encodedType.bits) + "From" + endian + "Bytes(data, &byteindex);\n";
        else
        {
            if(variableArray.isEmpty())
                output += spacing + "for(i = 0; i < " + array + "; i++)\n";
            else
            {
                if(isStructureMember)
                    output += spacing + "for(i = 0; i < (int)user->" + variableArray + " && i < " + array + "; i++)\n";
                else
                    output += spacing + "for(i = 0; i < (int)(*" + variableArray + ") && i < " + array + "; i++)\n";
            }

            output += spacing + "    " + lhs + name + "[i] = " + cast + opener + QString().setNum(encodedType.bits) + "From" + endian + "Bytes(data, &byteindex);\n";
        }

    }// else not scaled and not float

    // Close the default block
    if(defaultEnabled && !defaultValue.isEmpty())
    {
        // Remove the last four spaces
        spacing.remove(spacing.size()-4, 4);
        output += spacing + "}\n";
    }

    if(!dependsOn.isEmpty())
    {
        // Remove the last four spaces
        spacing.remove(spacing.size()-4, 4);
        output += spacing + "}\n";
    }

    return output;

}// ProtocolField::getDecodeStringForField


/*!
 * Get a properly formatted number string for a floating point number
 * \param number is the number to turn into a string
 * \param bits is the number of bits in memory for this string. 32 or less will prompt a 'f' suffix on the string
 * \return the string.
 */
QString ProtocolField::getNumberString(double number, int bits)
{
    QString string;

    string.setNum(number, 'g', 16);

    // Make sure we have a decimal point
    if(!string.contains("."))
        string += ".0";

    // Float suffix
    if(bits <= 32)
        string += "f";

    return string;
}
