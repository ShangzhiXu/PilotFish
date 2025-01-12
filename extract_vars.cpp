#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <iostream>
#include <set>
#include <string>
#include <algorithm>


// Function to remove 'sizeof' operators and type casts while retaining their inner content
std::string removeSizeofAndCasts(const std::string& code) {
    std::string modifiedCode = code;

    // Regex to match reinterpret_cast or static_cast or dynamic_cast or const_cast
    // Pattern breakdown:
    // (reinterpret_cast|static_cast|dynamic_cast|const_cast)      : Match the cast type
    // \s*<\s*([^>]+)\s*>                                        : Match the template parameter inside <>
    // \s*\(\s*([^()]+)\s*\)                                     : Match the expression inside ()
    std::regex cast_regex(R"((reinterpret_cast|static_cast|dynamic_cast|const_cast)\s*<\s*([^>]+)\s*>\s*\(\s*([^()]+)\s*\))");

    // Replace the entire cast expression with just the inner expression (third capture group)
    modifiedCode = std::regex_replace(code, cast_regex, "$3");

    // 2. Remove C-style casts by replacing them with their inner expressions
    // Pattern: (type) expression
    // Replacement: expression
    // This pattern handles types with pointers, references, namespaces, and templates
    std::regex c_cast_regex(R"(\(\s*[a-zA-Z_:][a-zA-Z0-9_:<>\*\&\s]*\s*\)\s*([a-zA-Z_][a-zA-Z0-9_]*)\b)");
    modifiedCode = std::regex_replace(modifiedCode, c_cast_regex, "$1");

    // 3. Replace sizeof(expression) with expression
    // Pattern: sizeof ( expression )
    // Replacement: expression
    std::regex sizeof_with_parentheses_regex(R"(sizeof\s*\(\s*([^()]+?)\s*\))");
    modifiedCode = std::regex_replace(modifiedCode, sizeof_with_parentheses_regex, "$1");

    // 4. Replace sizeof type with type
    // Pattern: sizeof type
    // Replacement: type
    // This pattern handles types with namespaces and template parameters
    std::regex sizeof_without_parentheses_regex(R"(sizeof\s+([a-zA-Z_:][a-zA-Z0-9_:<>]*))");
    modifiedCode = std::regex_replace(modifiedCode, sizeof_without_parentheses_regex, "$1");

    return modifiedCode;
}

// Function to check if the name element is a type in a cast expression
bool isTypeInCast(xmlNodePtr nameNode) {
    if (nameNode == NULL || nameNode->parent == NULL) {
        return false;
    }

    xmlNodePtr exprNode = nameNode->parent;
    if (xmlStrcmp(exprNode->name, BAD_CAST "expr") != 0) {
        return false;
    }

    // Check if exprNode has exactly three children: operator '(', name, operator ')'
    int childCount = 0;
    xmlNodePtr children[3] = {NULL, NULL, NULL};

    for (xmlNodePtr child = exprNode->children; child != NULL; child = child->next) {
        if (child->type == XML_ELEMENT_NODE) {
            if (childCount >= 3) {
                return false;
            }
            children[childCount++] = child;
        }
    }

    if (childCount != 3) {
        return false;
    }

    // Check the structure
    if (xmlStrcmp(children[0]->name, BAD_CAST "operator") == 0 &&
                                                             xmlStrcmp(children[1]->name, BAD_CAST "name") == 0 &&
                                                                                                              xmlStrcmp(children[2]->name, BAD_CAST "operator") == 0) {
        // Check that the operators are '(' and ')'
        xmlChar* op1 = xmlNodeGetContent(children[0]);
        xmlChar* op2 = xmlNodeGetContent(children[2]);
        if (op1 && op2 && xmlStrcmp(op1, BAD_CAST "(") == 0 && xmlStrcmp(op2, BAD_CAST ")") == 0) {
            xmlFree(op1);
            xmlFree(op2);
            return true;
        }
        if (op1) xmlFree(op1);
        if (op2) xmlFree(op2);
    }

    return false;
}

int main() {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<unit xmlns="http://www.srcML.org/srcML/src" xmlns:pos="http://www.srcML.org/srcML/position" revision="1.0.0" language="C++" pos:tabs="8"><decl pos:start="1:1" pos:end="1:40"><type pos:start="1:1" pos:end="1:7"><name pos:start="1:1" pos:end="1:7">Typedef</name></type> <name pos:start="1:9" pos:end="1:11">var</name> <init pos:start="1:13" pos:end="1:40">= <expr pos:start="1:15" pos:end="1:40"><operator pos:start="1:15" pos:end="1:15">(</operator><name pos:start="1:16" pos:end="1:18">int</name><operator pos:start="1:19" pos:end="1:19">)</operator><macro pos:start="1:20" pos:end="1:40"><name pos:start="1:20" pos:end="1:25">memcpy</name><argument_list pos:start="1:26" pos:end="1:40">(<argument pos:start="1:27" pos:end="1:27">a</argument>,<argument pos:start="1:29" pos:end="1:29">b</argument>,<argument pos:start="1:31" pos:end="1:39">sizeof(a)</argument>)</argument_list></macro></expr></init></decl>
</unit>
)";

    // Initialize libxml
    LIBXML_TEST_VERSION

    // Parse the XML from memory
    xmlDocPtr doc = xmlReadMemory(xml.c_str(), xml.length(), "noname.xml", NULL, XML_PARSE_NOBLANKS);
    if (doc == NULL) {
        std::cerr << "Error: unable to parse XML." << std::endl;
        return -1;
    }

    // Create XPath context
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (xpathCtx == NULL) {
        std::cerr << "Error: unable to create new XPath context." << std::endl;
        xmlFreeDoc(doc);
        return -1;
    }

    // Register the 'src' namespace with the prefix 'src'
    if (xmlXPathRegisterNs(xpathCtx, BAD_CAST "src", BAD_CAST "http://www.srcML.org/srcML/src") != 0) {
        std::cerr << "Error: unable to register namespace." << std::endl;
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return -1;
    }

    // Prepare a set to store variable names
    std::set<std::string> varNames;

    // XPath expression to find all <name> elements not under <type>, <call>, or <macro> elements
    const xmlChar* xpathExpr = BAD_CAST "//src:name[not(ancestor::src:type) and not(parent::src:call) and not(parent::src:macro) and not(parent::src:operator)]";

    xmlXPathObjectPtr nameNodesResult = xmlXPathEvalExpression(xpathExpr, xpathCtx);
    if (nameNodesResult && nameNodesResult->nodesetval) {
        int size = nameNodesResult->nodesetval->nodeNr;
        for (int i = 0; i < size; ++i) {
            xmlNodePtr node = nameNodesResult->nodesetval->nodeTab[i];
            // Include the name
            xmlChar* content = xmlNodeGetContent(node);
            if (content) {
                varNames.insert((char*)content);
                xmlFree(content);
            }
        }
    }
    xmlXPathFreeObject(nameNodesResult);

    // Also process <argument> elements that may directly contain variable names
    xmlXPathObjectPtr argumentNodesResult = xmlXPathEvalExpression(
    BAD_CAST "//src:argument",
            xpathCtx);
    if (argumentNodesResult && argumentNodesResult->nodesetval) {
        int size = argumentNodesResult->nodesetval->nodeNr;
        for (int i = 0; i < size; ++i) {
            xmlNodePtr node = argumentNodesResult->nodesetval->nodeTab[i];
            xmlChar* content = xmlNodeGetContent(node);
            if (content) {
                // Remove whitespace
                std::string value((char*)content);
                value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
                if (!value.empty()) {
                    varNames.insert(value);
                }
                xmlFree(content);
            }
        }
    }
    xmlXPathFreeObject(argumentNodesResult);

    // Output the variable names
    for (const auto& name : varNames) {
        std::cout << "- " << name << std::endl;
    }

    // Cleanup
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return 0;
}
