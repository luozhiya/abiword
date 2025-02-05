/* AbiSource
 * 
 * Copyright (C) 2005 INdT
 * Author: Daniel d'Andrada T. de Carvalho <daniel.carvalho@indt.org.br>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301 USA.
 */
 
// Class definition include
#include "ODe_Text_Listener.h"

// Internal includes
#include "ODe_AutomaticStyles.h"
#include "ODe_AuxiliaryData.h"
#include "ODe_Common.h"
#include "ODe_Frame_Listener.h"
#include "ODe_ListenerAction.h"
#include "ODe_ListLevelStyle.h"
#include "ODe_Note_Listener.h"
#include "ODe_Styles.h"
#include "ODe_Style_List.h"
#include "ODe_Style_Style.h"
#include "ODe_Table_Listener.h"
#include "ODe_Style_PageLayout.h"

// AbiWord includes
#include <pp_AttrProp.h>
#include <ut_units.h>
#include <fl_TOCLayout.h>
#include <pd_DocumentRDF.h>
#include <pd_Document.h>

// External includes
#include <stdlib.h>
#include <libxml/uri.h>

#include <sstream>

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gsf/gsf-output.h>
#include <gsf/gsf-outfile.h>
#include <gsf/gsf-outfile-stdio.h>
#include <gsf/gsf-output-memory.h>

/**
 * Constructor
 * 
 * @param pTextOutput Handle to the file (often a temp one) that will receive 
 *                    the ODT output produced by this listener.
 */
ODe_Text_Listener::ODe_Text_Listener(ODe_Styles& rStyles,
                                     ODe_AutomaticStyles& rAutomatiStyles,
                                     GsfOutput* pTextOutput,
                                     ODe_AuxiliaryData& rAuxiliaryData,
                                     UT_uint8 zIndex,
                                     UT_uint8 spacesOffset) :
                        ODe_AbiDocListenerImpl(spacesOffset),
                        m_openedODParagraph(false),
                        m_openedODSpan(false),
                        m_isFirstCharOnParagraph(true),
                        m_openedODTextboxFrame(false),
                        m_openedODNote(false),
                        m_bIgoreFirstTab(false),
                        m_pParagraphContent(NULL),
                        m_currentListLevel(0),
                        m_pCurrentListStyle(NULL),
                        m_pendingColumnBreak(false),
                        m_pendingPageBreak(false),
                        m_bAfter(false),
                        m_pendingMasterPageStyleChange(false),
                        m_rStyles(rStyles),
                        m_rAutomatiStyles(rAutomatiStyles),
                        m_pTextOutput(pTextOutput),
                        m_rAuxiliaryData(rAuxiliaryData),
                        m_zIndex(zIndex),
                        m_iCurrentTOC(0)
{
	_initDefaultHeadingStyles();
}


/**
 * Constructor
 * 
 * @param pTextOutput Handle to the file (often a temp one) that will receive 
 *                    the ODT output produced by this listener.
 */
ODe_Text_Listener::ODe_Text_Listener(ODe_Styles& rStyles,
                             ODe_AutomaticStyles& rAutomatiStyles,
                             GsfOutput* pTextOutput,
                             ODe_AuxiliaryData& rAuxiliaryData,
                             UT_uint8 zIndex,
                             UT_uint8 spacesOffset,
                             const UT_UTF8String& rPendingMasterPageStyleName) :
                        ODe_AbiDocListenerImpl(spacesOffset),
                        m_openedODParagraph(false),
                        m_openedODSpan(false),
                        m_isFirstCharOnParagraph(true),
                        m_openedODTextboxFrame(false),
                        m_openedODNote(false),
			m_bIgoreFirstTab(false),
                        m_pParagraphContent(NULL),
                        m_currentListLevel(0),
                        m_pCurrentListStyle(NULL),
                        m_pendingColumnBreak(false),
                        m_pendingPageBreak(false),
                        m_pendingMasterPageStyleChange(true),
                        m_masterPageStyleName(rPendingMasterPageStyleName),
                        m_rStyles(rStyles),
                        m_rAutomatiStyles(rAutomatiStyles),
                        m_pTextOutput(pTextOutput),
                        m_rAuxiliaryData(rAuxiliaryData),
                        m_zIndex(zIndex),
                        m_iCurrentTOC(0)
{
	_initDefaultHeadingStyles();
}


/**
 * 
 */
ODe_Text_Listener::~ODe_Text_Listener() {
    // Check if there is nothing being left unfinished.
    
    UT_ASSERT_HARMLESS(!m_openedODParagraph);
    UT_ASSERT_HARMLESS(!m_openedODSpan);

    UT_ASSERT_HARMLESS(m_currentListLevel == 0);
    UT_ASSERT_HARMLESS(m_pCurrentListStyle == NULL);
}


/**
 * 
 */
void ODe_Text_Listener::openTable(const PP_AttrProp* /*pAP*/,
                                  ODe_ListenerAction& rAction) {
    _closeODParagraph();
    _closeODList();

    rAction.pushListenerImpl(new ODe_Table_Listener(m_rStyles,
                                                    m_rAutomatiStyles,
                                                    m_pTextOutput,
                                                    m_rAuxiliaryData,
                                                    0,
                                                    m_spacesOffset),
                             true);
}


/**
 * Override of ODe_AbiDocListenerImpl::openBlock
 */
void ODe_Text_Listener::openBlock(const PP_AttrProp* pAP,
                                  ODe_ListenerAction& /*rAction*/) {

    _closeODParagraph();

    // We first handle the list info of that paragraph, if it exists.
    _openODListItem(pAP);
    
    // Then we try to open an OpenDocument paragraph out of this AbiWord block.
    _openODParagraph(pAP);

}


/**
 * 
 */
void ODe_Text_Listener::closeBlock() {
}


/**
 * 
 */
void ODe_Text_Listener::openSpan(const PP_AttrProp* pAP) {
    UT_UTF8String styleName;
    bool ok;
    const gchar* pValue;
    
    if ( ODe_Style_Style::hasTextStyleProps(pAP) ) {
            
        // Need to create a new automatic style to hold those paragraph
        // properties.
        
        ODe_Style_Style* pStyle;
        pStyle = new ODe_Style_Style();
        pStyle->setFamily("text");
        
        pStyle->fetchAttributesFromAbiSpan(pAP);
        
        m_rAutomatiStyles.storeTextStyle(pStyle);
        styleName = pStyle->getName();
        
    } else {
        ok = pAP->getAttribute("style", pValue);
        if (ok) {
            styleName = pValue;
        }
    }
    
    if (!styleName.empty()) {
        UT_UTF8String output;
        
        UT_UTF8String_sprintf(output, "<text:span text:style-name=\"%s\">",
                              ODe_Style_Style::convertStyleToNCName(styleName).escapeXML().utf8_str());
                              
        ODe_writeUTF8String(m_pParagraphContent, output);
        m_openedODSpan = true;
    }
}


/**
 * 
 */
void ODe_Text_Listener::closeSpan() {
    if (m_openedODSpan) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:span>");
        m_openedODSpan = false;
    }
}


/**
 * 
 */
void ODe_Text_Listener::openFrame(const PP_AttrProp* pAP,
                                  ODe_ListenerAction& rAction) {
    bool ok = false;
    const gchar* pValue = NULL;
    
    ok = pAP->getProperty("frame-type", pValue);
    
    if (pValue && !strcmp(pValue, "textbox")) {
        ODe_Frame_Listener* pFrameListener;
            
        // Paragraph anchored textboxes goes inside the paragraph that
        // it's anchored to, right before the paragraph contents.
        //
        // Page anchored textboxes goes inside the closest previous paragraph.

        pFrameListener = new ODe_Frame_Listener(m_rStyles,
                                                m_rAutomatiStyles,
                                                m_pTextOutput,
                                                m_rAuxiliaryData,
                                                m_zIndex,
                                                m_spacesOffset);
                                                
        // Make the frame element appear on a new line
        ODe_writeUTF8String(m_pTextOutput, "\n");
                                                
        rAction.pushListenerImpl(pFrameListener, true);
        m_openedODTextboxFrame = true;
    } else if (pValue && !strcmp(pValue, "image")) {
        ok = pAP->getAttribute(PT_STRUX_IMAGE_DATAID, pValue);
        if(ok && pValue) {
            insertPositionedImage(pValue, pAP);
        }
        m_openedODTextboxFrame = true;
    }
}


/**
 * 
 */
void ODe_Text_Listener::closeFrame(ODe_ListenerAction& rAction) {

    if (m_openedODTextboxFrame) {
        m_openedODTextboxFrame = false;
    } else {
        // We are inside a textbox.        
        _closeODParagraph();
        rAction.popListenerImpl();
    }
}


/**
 * 
 */
void ODe_Text_Listener::openField(const fd_Field* field, const UT_UTF8String& fieldType, const UT_UTF8String& fieldValue) {
    UT_return_if_fail(field && fieldType.length());

    UT_UTF8String escape = fieldValue;
    escape.escapeXML();

    if(!strcmp(fieldType.utf8_str(),"list_label")) {
        return;  // don't do anything with list labels
    } else if(!strcmp(fieldType.utf8_str(),"page_number")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:page-number>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"page_count")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:page-count>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"meta_creator")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:author-name>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"meta_title")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:title>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"meta_description")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:description>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"meta_subject")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:subject>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"meta_keywords")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:keywords>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"char_count")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:character-count>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"word_count")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:word-count>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"para_count")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:paragraph-count>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"file_name")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:file-name>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"time")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:time>%s",escape.utf8_str()));
    } else if(!strcmp(fieldType.utf8_str(),"date")) {
        ODe_writeUTF8String(m_pParagraphContent, UT_UTF8String_sprintf("<text:date>%s",escape.utf8_str()));
    } else {
        UT_DEBUGMSG(("openField(): Unhandled field in the ODT exporter: %s\n", fieldType.utf8_str()));
    }
}

/**
 * 
 */

void ODe_Text_Listener::closeField(const UT_UTF8String& fieldType) {
    UT_return_if_fail(fieldType.length());

    if(!strcmp(fieldType.utf8_str(),"list_label")) {
        return;  // don't do anything with list labels
    } else if(!strcmp(fieldType.utf8_str(),"page_number")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:page-number>");
    } else if(!strcmp(fieldType.utf8_str(),"page_count")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:page-count>");
    } else if(!strcmp(fieldType.utf8_str(),"meta_creator")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:author-name>");
    } else if(!strcmp(fieldType.utf8_str(),"meta_title")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:title>");
    } else if(!strcmp(fieldType.utf8_str(),"meta_description")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:description>");
    } else if(!strcmp(fieldType.utf8_str(),"meta_subject")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:subject>");
    } else if(!strcmp(fieldType.utf8_str(),"meta_keywords")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:keywords>");
    } else if(!strcmp(fieldType.utf8_str(),"char_count")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:character-count>");
    } else if(!strcmp(fieldType.utf8_str(),"word_count")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:word-count>");
    } else if(!strcmp(fieldType.utf8_str(),"para_count")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:paragraph-count>");
    } else if(!strcmp(fieldType.utf8_str(),"file_name")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:file-name>");
    } else if(!strcmp(fieldType.utf8_str(),"time")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:time>");
    } else if(!strcmp(fieldType.utf8_str(),"date")) {
        ODe_writeUTF8String(m_pParagraphContent, "</text:date>");
    } else {
        UT_DEBUGMSG(("closeField(): Unhandled field in the ODT exporter: %s\n", fieldType.utf8_str()));
    }
}

/**
 * 
 */


void ODe_Text_Listener::openFootnote(const PP_AttrProp* /*pAP*/,
                                     ODe_ListenerAction& rAction) {
    ODe_Note_Listener* pNoteListener;
    
    pNoteListener = new ODe_Note_Listener(m_rStyles,
                                          m_rAutomatiStyles,
                                          m_pParagraphContent,
                                          m_rAuxiliaryData,
                                          m_spacesOffset);
    
    rAction.pushListenerImpl(pNoteListener, true);
    m_openedODNote = true;
}


/**
 * 
 */
void ODe_Text_Listener::closeFootnote(ODe_ListenerAction& rAction) {
    if (m_openedODNote) {
        // We had a footnote.        
        m_openedODNote = false;
    } else {
        // We were inside a footnote.
        _closeODParagraph();
        _closeODList();
        rAction.popListenerImpl();
    }
}


/**
 * 
 */
void ODe_Text_Listener::openEndnote(const PP_AttrProp* /*pAP*/,
                                    ODe_ListenerAction& rAction) {
    ODe_Note_Listener* pNoteListener;
    
    pNoteListener = new ODe_Note_Listener(m_rStyles,
                                          m_rAutomatiStyles,
                                          m_pParagraphContent,
                                          m_rAuxiliaryData,
                                          m_spacesOffset);
    
    rAction.pushListenerImpl(pNoteListener, true);
    m_openedODNote = true;
}


/**
 * 
 */
void ODe_Text_Listener::closeEndnote(ODe_ListenerAction& rAction) {
    if (m_openedODNote) {
        // We had a endnote.        
        m_openedODNote = false;
    } else {
        // We were inside an endnote.
        _closeODParagraph();
        _closeODList();
        rAction.popListenerImpl();
    }
}


/**
 * 
 */
void ODe_Text_Listener::openAnnotation(const PP_AttrProp* pAP, const std::string& name, PD_Document* doc )
{
    UT_UTF8String output = "<office:annotation";
    UT_UTF8String escape;
    const gchar* pValue = NULL;

    UT_UTF8String generatedID;
    const gchar* xmlid = nullptr;
    if(pAP && pAP->getProperty(PT_XMLID,pValue) && pValue && *pValue)
    {
        xmlid = pValue;
    }
    else if( doc )
    {
        generatedID = UT_UTF8String_sprintf("anno%d", doc->getUID( UT_UniqueId::Annotation ));
        xmlid = generatedID.utf8_str();
        UT_DEBUGMSG(("openAnno xmlid:%s\n", xmlid ));
    }
    if( xmlid )
    {
        appendAttribute( output, "xml:id", xmlid );

        //
        // MIQ: Lets put the annotation-title into the document RDF ;)
        //
        if(pAP && pAP->getProperty("annotation-title",pValue) && pValue && *pValue)
        {
            std::string title = pValue;
            UT_DEBUGMSG(("openAnno title:%s\n", title.c_str() ));

            PD_RDFModelHandle rdf = m_rAuxiliaryData.m_additionalRDF;
//            PD_DocumentRDFHandle rdf = doc->getDocumentRDF();
            PD_DocumentRDFMutationHandle m = rdf->createMutation();

            PD_URI subj = m->createBNode();
            PD_URI pkg_idref("http://docs.oasis-open.org/opendocument/meta/package/common#idref");
            PD_URI dc_title("http://purl.org/dc/elements/1.1/title");
            //  ?s pkg:idref ?xmlid . 
            //  ?s ?p ?o . 
            m->add( subj, pkg_idref, PD_Literal( xmlid ) );
            m->add( subj, dc_title,  PD_Literal( title ) );
            m->commit();
        }
    }
    

    if( !name.empty() )
    {
        output += " office:name=\"";
        output += name.c_str();
        output += "\"";
    }
    output += ">";
    
    if(pAP && pAP->getProperty("annotation-author",pValue) && pValue && *pValue) {
        escape = pValue;
        escape.escapeXML();

        output += "<dc:creator>";
        output += escape;
        output += "</dc:creator>";
    }

    if(pAP && pAP->getProperty("annotation-date",pValue) && pValue && *pValue) {
        escape = ODc_reorderDate(pValue, false);
        escape.escapeXML();

        // TODO: is our property a valid date value?
        output += "<dc:date>";
        output += escape;
        output += "</dc:date>";
    }


    ODe_writeUTF8String(m_pParagraphContent, output);
}

/**
 * 
 */
void ODe_Text_Listener::closeAnnotation( const std::string& /*name*/ )
{
    UT_UTF8String output = "</office:annotation>";
    ODe_writeUTF8String(m_pParagraphContent, output);
}

void ODe_Text_Listener::endAnnotation( const std::string& name )
{
    std::stringstream ss;
    ss << "<office:annotation-end  office:name=\"" << name << "\"/>";
    ODe_write( m_pParagraphContent, ss );
}


/**
 * 
 */
void ODe_Text_Listener::openTOC(const PP_AttrProp* pAP) {
    UT_UTF8String output;
    bool ok;
    const gchar* pValue = nullptr;
    UT_uint8 outlineLevel;
    UT_UTF8String str;
    
    _closeODParagraph();
    _closeODList();
    
	m_iCurrentTOC++;
    
    ////
    // Write <text:table-of-content>, <text:table-of-content-source> and <text:index-body>
    
    str.clear();
    _printSpacesOffset(str);
    
    UT_UTF8String tocName;
    UT_UTF8String_sprintf(tocName, "Table of Contents %u", m_iCurrentTOC);
    tocName.escapeXML();
    
    UT_UTF8String_sprintf(output,
        "%s<text:table-of-content text:protected=\"true\""
        " text:name=\"%s\">\n",
        str.utf8_str(), tocName.utf8_str());
   
    ODe_writeUTF8String(m_pTextOutput, output);
    m_spacesOffset++;
    output.assign("");
    
    
    _printSpacesOffset(output);
    output += "<text:table-of-content-source text:outline-level=\"4\">\n";
    
    ODe_writeUTF8String(m_pTextOutput, output);
    m_spacesOffset++;
    output.assign("");
    
    ////
    // Write <text:index-title-template>
    
    bool hasHeading = true; // AbiWord's default
    ok = pAP->getProperty("toc-has-heading", pValue);
    if (ok && pValue) {
        hasHeading = (*pValue == '1');
    }

    // determine the style of the TOC heading
    UT_UTF8String headingStyle;
    ok = pAP->getProperty("toc-heading-style", pValue);
    if (ok && pValue) {
        headingStyle = pValue;
    } else {
        const PP_Property* pProp = PP_lookupProperty("toc-heading-style");
        UT_ASSERT_HARMLESS(pProp);
        if (pProp)
            headingStyle = pProp->getInitial();
    }

    if (hasHeading) {
        // make sure this TOC headering style is exported to the ODT style list
        m_rStyles.addStyle(headingStyle);
    }

    // determine the contents of the TOC heading
    UT_UTF8String tocHeading;
    ok = pAP->getProperty("toc-heading", pValue);
    if (ok && pValue) {
        tocHeading = pValue;
    } else {
        tocHeading = fl_TOCLayout::getDefaultHeading();
    }

    if (hasHeading) {
        _printSpacesOffset(output);
        output += "<text:index-title-template text:style-name=\"";
        output += ODe_Style_Style::convertStyleToNCName(headingStyle).escapeXML();     
        output += "\">";
        output += tocHeading.escapeXML();        
        output += "</text:index-title-template>\n";
        
        ODe_writeUTF8String(m_pTextOutput, output);
        output.assign("");
    }
    
    
    ////
    // Write all <text:table-of-content-entry-template>
    
    for (outlineLevel=1; outlineLevel<=4; outlineLevel++) {

        str.assign("");
        _printSpacesOffset(str);
        
        UT_UTF8String_sprintf(output,
            "%s<text:table-of-content-entry-template"
            " text:outline-level=\"%u\" text:style-name=\"",
            str.utf8_str(), outlineLevel);

        UT_UTF8String destStyle = m_rAuxiliaryData.m_mDestStyles[outlineLevel];
        UT_ASSERT_HARMLESS(destStyle != "");
        output += ODe_Style_Style::convertStyleToNCName(destStyle).escapeXML();
        
        output += "\">\n";
        m_spacesOffset++;
        
        // Fixed TOC structure (at least for now).
        // [chapter][text]............[page-number]
        
        _printSpacesOffset(output);
        output += "<text:index-entry-chapter/>\n";
        
        _printSpacesOffset(output);
        output += "<text:index-entry-text/>\n";
        
        _printSpacesOffset(output);
        output += "<text:index-entry-tab-stop style:type=\"right\""
                  " style:leader-char=\".\"/>\n";
                  
        _printSpacesOffset(output);
        output += "<text:index-entry-page-number/>\n";
        
        m_spacesOffset--;
        _printSpacesOffset(output);
        output += "</text:table-of-content-entry-template>\n";
        
        ODe_writeUTF8String(m_pTextOutput, output);
        output.assign("");
    }

    m_spacesOffset--;
    _printSpacesOffset(output);
    output += "</text:table-of-content-source>\n";
    ODe_writeUTF8String(m_pTextOutput, output);

    ////
	// write <text:index-body>
    UT_ASSERT_HARMLESS(m_rAuxiliaryData.m_pTOCContents);
    if (m_rAuxiliaryData.m_pTOCContents) {
        output.assign("");
        _printSpacesOffset(output);

        output += "<text:index-body>\n";
        ODe_writeUTF8String(m_pTextOutput, output);
        output.assign("");

        m_spacesOffset++;

        if (hasHeading) {
            _printSpacesOffset(output);
            output += "<text:index-title text:name=\"";
            output += tocName; // the text:name field is required and should be unique
            output += "\">\n";

            m_spacesOffset++;
            _printSpacesOffset(output);
            output += "<text:p text:style-name=\"";
            output += ODe_Style_Style::convertStyleToNCName(headingStyle).escapeXML();
            output += "\">";
            output += tocHeading.escapeXML();
            output += "</text:p>\n"; 

            m_spacesOffset--;
            _printSpacesOffset(output);
            output += "</text:index-title>\n";

            ODe_writeUTF8String(m_pTextOutput, output);
            output.assign("");
        }

        gsf_output_write(m_pTextOutput, gsf_output_size(m_rAuxiliaryData.m_pTOCContents),
                gsf_output_memory_get_bytes(GSF_OUTPUT_MEMORY(m_rAuxiliaryData.m_pTOCContents)));

        m_spacesOffset--;
        _printSpacesOffset(output);
        output += "</text:index-body>\n";

        ODe_writeUTF8String(m_pTextOutput, output);
        output.assign("");
    }
}


/**
 * 
 */
void ODe_Text_Listener::closeTOC() {
    UT_UTF8String output;
    
    m_spacesOffset--;
    _printSpacesOffset(output);
    output += "</text:table-of-content>\n";
    ODe_writeUTF8String(m_pTextOutput, output);
}


/**
 * 
 */
void ODe_Text_Listener::openBookmark(const PP_AttrProp* pAP) {
    UT_return_if_fail(pAP);

    UT_UTF8String output = "<text:bookmark-start text:name=\"", escape;
    const gchar* pValue = NULL;

    if(pAP->getAttribute("type",pValue) && pValue && (strcmp(pValue, "start") == 0)) {
        if(pAP->getAttribute("name",pValue) && pValue) {
            escape = pValue;
            escape.escapeXML();

            if(escape.length()) {
                output+= escape;
                output+="\" ";

                const char* xmlid = nullptr;
                if( pAP->getAttribute( PT_XMLID, xmlid ) && xmlid )
                {
                    appendAttribute( output, "xml:id", xmlid );
                }

                output+=" />";
                ODe_writeUTF8String(m_pParagraphContent, output);
            }
        }
    }
}

/**
 * 
 */
void ODe_Text_Listener::closeBookmark(const PP_AttrProp* pAP) {
    UT_return_if_fail(pAP);

    UT_UTF8String output = "<text:bookmark-end text:name=\"", escape;
    const gchar* pValue = NULL;

    if(pAP->getAttribute("type",pValue) && pValue && (strcmp(pValue, "end") == 0)) {
        if(pAP->getAttribute("name",pValue) && pValue) {
            escape = pValue;
            escape.escapeXML();

            if(escape.length()) {
                output+= escape;
                output+="\"/>";
                ODe_writeUTF8String(m_pParagraphContent, output);
            }
        }
    }
}


/**
 * 
 */
void ODe_Text_Listener::closeBookmark(UT_UTF8String &sBookmarkName) {
    UT_return_if_fail(sBookmarkName.length());

    UT_UTF8String output = "<text:bookmark-end text:name=\"", escape;
    escape = sBookmarkName;
    escape.escapeXML();

    if(escape.length()) {
        output+= escape;
        output+="\"/>";
        ODe_writeUTF8String(m_pParagraphContent, output);
    }
}


/**
 * 
 */
void ODe_Text_Listener::openHyperlink(const PP_AttrProp* pAP) {
    UT_return_if_fail(pAP);

    const gchar* pValue = NULL;
    const gchar* pTitle = NULL;
    bool bHaveTitle = false;

    bHaveTitle = pAP->getAttribute("xlink:title", pTitle) && pTitle;

    if(pAP->getAttribute("xlink:href",pValue) && pValue) {
        UT_UTF8String uri = pValue;
        uri.escapeXML();

        if (uri.length() > 0)
        {
            UT_UTF8String output = "<text:a ";
            if (bHaveTitle)
            {
                output += "office:title=\"";
                output += pTitle;
                output += "\" ";
            }
            output+="xlink:href=\"";
            output+=uri;
            output+="\">";
            ODe_writeUTF8String(m_pParagraphContent, output);
        }
    }
}

/**
 * 
 */
void ODe_Text_Listener::closeHyperlink() {
    UT_UTF8String output = "</text:a>";
    ODe_writeUTF8String(m_pParagraphContent, output);
}

void ODe_Text_Listener::openRDFAnchor(const PP_AttrProp* pAP)
{
    UT_return_if_fail(pAP);
    RDFAnchor a(pAP);
    
    UT_UTF8String output = "<text:meta ";
    UT_UTF8String escape = a.getID().c_str();
    escape.escapeURL();
    
    output+=" xml:id=\"";
    output+= escape;
    output+="\" ";
    output+=" >";
    ODe_writeUTF8String(m_pParagraphContent, output);
}


void ODe_Text_Listener::closeRDFAnchor(const PP_AttrProp* pAP)
{
    RDFAnchor a(pAP);
    UT_UTF8String output = "</text:meta>";
    ODe_writeUTF8String(m_pParagraphContent, output);
}

/**
 * 
 */
void ODe_Text_Listener::insertText(const UT_UTF8String& rText) {
	if (rText.length() == 0)
		return;
    ODe_writeUTF8String(m_pParagraphContent, rText);
    m_isFirstCharOnParagraph = false;
}


/**
 * 
 */
void ODe_Text_Listener::closeCell(ODe_ListenerAction& rAction) {
    _closeODParagraph();
    _closeODList(); // Close the current list, if there is one.
    rAction.popListenerImpl();
}


/**
 * 
 */
void ODe_Text_Listener::closeSection(ODe_ListenerAction& rAction) {
    _closeODParagraph();
    _closeODList(); // Close the current list, if there is one.
    rAction.popListenerImpl();
}


/**
 *
 */
void ODe_Text_Listener::insertLineBreak() {
    ODe_writeUTF8String(m_pParagraphContent, "<text:line-break/>");
}


/**
 *
 */
void ODe_Text_Listener::insertColumnBreak() {
    _closeODList();
    m_pendingColumnBreak = true;
    if (!m_isFirstCharOnParagraph){
        m_bAfter = true;
    }
}


/**
 *
 */
void ODe_Text_Listener::insertPageBreak() {
    _closeODList();
    m_pendingPageBreak = true;
    if (!m_isFirstCharOnParagraph){
        m_bAfter = true;
    }
}


/**
 *
 */
void ODe_Text_Listener::insertTabChar() {
    // We will not write the tab char that abi inserts right after each
    // list item bullet/number.
  // Also don't write out the tab after a note anchor
    
  if (!m_bIgoreFirstTab && (!m_isFirstCharOnParagraph || (m_currentListLevel == 0)))
    {
      ODe_writeUTF8String(m_pParagraphContent, "<text:tab/>");
    }

    m_isFirstCharOnParagraph = false;
    m_bIgoreFirstTab = false;
}


/**
 *
 */
void ODe_Text_Listener::insertInlinedImage(const gchar* pImageName,
                                           const PP_AttrProp* pAP) 
{
    UT_UTF8String output;
    UT_UTF8String str;
    UT_UTF8String escape;
    ODe_Style_Style* pStyle;
    const gchar* pValue;
    bool ok;

    
    pStyle = new ODe_Style_Style();
    pStyle->setFamily("graphic");
    pStyle->setWrap("run-through");
    pStyle->setRunThrough("foreground");
    //
    // inline images are always "baseline" vertical-rel and top vertical-rel
    //
    pStyle->setVerticalPos("top");
    pStyle->setVerticalRel("baseline");
    // For OOo to recognize an image as being an image, it will
    // need to have the parent style name "Graphics". I can't find it
    // in the ODF spec, but without it OOo doesn't properly recognize
	// it (check the Navigator window in OOo).
    pStyle->setParentStyleName("Graphics");
    // make sure an (empty) Graphics style exists, for completeness sake
	// (OOo doesn't seem to care if it exists or not)
    if (!m_rStyles.getGraphicsStyle("Graphics")) {
        ODe_Style_Style* pGraphicsStyle = new ODe_Style_Style();
		pGraphicsStyle->setStyleName("Graphics");
		pGraphicsStyle->setFamily("graphic");
        m_rStyles.addGraphicsStyle(pGraphicsStyle);
    }

	m_rAutomatiStyles.storeGraphicStyle(pStyle);
    
    output = "<draw:frame text:anchor-type=\"as-char\"";

    UT_UTF8String_sprintf(str, "%u", m_zIndex);
    ODe_writeAttribute(output, "draw:z-index", str);
    ODe_writeAttribute(output, "draw:style-name", pStyle->getName());

    ok = pAP->getProperty("width", pValue);
    if (ok && pValue != NULL) {
        ODe_writeAttribute(output, "svg:width", pValue);
    }
    
    ok = pAP->getProperty("height", pValue);
    if (ok && pValue != NULL) {
        ODe_writeAttribute(output, "svg:height", pValue);
    }
    output += "><draw:image xlink:href=\"Pictures/";
    output += pImageName;
    output += "\" xlink:type=\"simple\" xlink:show=\"embed\""
              " xlink:actuate=\"onLoad\"/>";

    ok = pAP->getAttribute("title", pValue);
    if (ok && pValue != NULL) {
       escape = pValue;
       escape.escapeXML();
       if(escape.length()) {
           output += "<svg:title>";
           output += escape.utf8_str();
           output += "</svg:title>";
       }
    }

    ok = pAP->getAttribute("alt", pValue);
    if (ok && pValue != NULL) {
       escape = pValue;
       escape.escapeXML();
       if(escape.length()) {
           output += "<svg:desc>";
           output += escape.utf8_str();
           output += "</svg:desc>";
       }
       escape.clear();
    }

    output += "</draw:frame>";

    ODe_writeUTF8String(m_pParagraphContent, output);
}


void ODe_Text_Listener::insertPositionedImage(const gchar* pImageName,
                                                 const PP_AttrProp* pAP)
{
    UT_UTF8String output = "<text:p>";
    UT_UTF8String str;
    UT_UTF8String escape;
    ODe_Style_Style* pStyle;
    const gchar* pValue;
    bool ok;
   
    pStyle = new ODe_Style_Style();
    pStyle->setFamily("graphic");
    // For OOo to recognize an image as being an image, it will
    // need to have the parent style name "Graphics". I can't find it
    // in the ODF spec, but without it OOo doesn't properly recognize
	// it (check the Navigator window in OOo).
    pStyle->setParentStyleName("Graphics");

    //set wrapping
    ok = pAP->getProperty("wrap-mode", pValue);
    if(ok && pValue && !strcmp(pValue, "wrapped-to-right")) {
        pStyle->setWrap("right");
    }
    else if(ok && pValue && !strcmp(pValue, "wrapped-to-left")) {
        pStyle->setWrap("left");
    }
    else if(ok && pValue && !strcmp(pValue, "wrapped-both")) {
        pStyle->setWrap("parallel");
    }
    else { //this handles the above-text case and any other unforeseen ones
        pStyle->setWrap("run-through");
        pStyle->setRunThrough("foreground");
    }

    m_rAutomatiStyles.storeGraphicStyle(pStyle);    
    
    output += "<draw:frame text:anchor-type=\"";
    ok = pAP->getProperty("position-to", pValue);
    if(ok && pValue && !strcmp(pValue, "column-above-text")) {
        output+="page\""; //the spec doesn't seem to handle column anchoring
	// we work around it
	ok = pAP->getProperty("pref-page", pValue);
 	if(ok)
	{
	    UT_sint32 iPage = atoi(pValue)+1;
	    UT_UTF8String sPage;
	    UT_UTF8String_sprintf(sPage,"%d",iPage);
	    ODe_writeAttribute(output, "text:anchor-page-number", sPage.utf8_str());
	}
	else
	{
	    ODe_writeAttribute(output, "text:anchor-page-number", "1");
	}
	//
	// Get the most recent page style so we can do the arithmetic
	// Won't work for x in multi-columned docs
	//

	UT_DEBUGMSG(("InsertPosionedObject TextListener %p AutoStyle %p \n",this,&m_rAutomatiStyles));
	ODe_Style_PageLayout * pPageL = NULL;
	UT_uint32 numPStyles =  m_rAutomatiStyles.getSectionStylesCount();
	UT_UTF8String stylePName;
	UT_DEBUGMSG(("Number PageLayoutStyles %d \n",numPStyles));
	UT_UTF8String_sprintf(stylePName, "PLayout%d", numPStyles + 1);
	pPageL = m_rAutomatiStyles.getPageLayout(stylePName.utf8_str());
	if(pPageL == NULL)
	{
	    pPageL = m_rAutomatiStyles.getPageLayout("Standard");
	}
	UT_DEBUGMSG(("Got PageLayoutStyle %p \n",pPageL));
	double xPageL = 0.;
	double yPageL = 0.;
	
	ok = pAP->getProperty("frame-col-xpos", pValue);
	UT_ASSERT(ok && pValue != NULL);
	double xCol =  UT_convertToInches(pValue);
	const gchar* pSVal= NULL;
	if(pPageL)
	{
	    pSVal = pPageL->getPageMarginLeft();
	    xPageL = UT_convertToInches(pSVal);
	}
	double xTot = xPageL + xCol;
	pValue = UT_convertInchesToDimensionString(DIM_IN,xTot,"4");
	ODe_writeAttribute(output, "svg:x", pValue);
        
	ok = pAP->getProperty("frame-col-ypos", pValue);
	UT_ASSERT(ok && pValue != NULL);
	double yCol =  UT_convertToInches(pValue);
	if(pPageL)
	{
	    pSVal = pPageL->getPageMarginTop();
	    yPageL = UT_convertToInches(pSVal);
	    pSVal = pPageL->getPageMarginHeader();
	    yPageL += UT_convertToInches(pSVal);
	    UT_DEBUGMSG(("PageMarginTop %s Margin in %f8.4\n",pSVal,yPageL));
	}
	double yTot = yPageL + yCol;
	UT_DEBUGMSG(("Col %f8.4 Total in %f8.4\n",yCol,yTot));
	pValue = UT_convertInchesToDimensionString(DIM_IN,yTot,"4");
	ODe_writeAttribute(output, "svg:y", pValue);
    }
    else if(ok && pValue && !strcmp(pValue, "page-above-text")) {
        output+="page\"";
        ok = pAP->getProperty("frame-page-xpos", pValue);
        if (ok && pValue != NULL)
            ODe_writeAttribute(output, "svg:x", pValue);
        
        ok = pAP->getProperty("frame-page-ypos", pValue);
        if (ok && pValue != NULL)
            ODe_writeAttribute(output, "svg:y", pValue);
    }
    else { //this handles the block-above-text case and any other unforeseen ones
        output+="paragraph\"";
        ok = pAP->getProperty("xpos", pValue);
        if (ok && pValue != NULL)
            ODe_writeAttribute(output, "svg:x", pValue);
        
        ok = pAP->getProperty("ypos", pValue);
        if (ok && pValue != NULL)
            ODe_writeAttribute(output, "svg:y", pValue);
    }

    UT_UTF8String_sprintf(str, "%u", m_zIndex);
    ODe_writeAttribute(output, "draw:z-index", str);
    ODe_writeAttribute(output, "draw:style-name", pStyle->getName());

    ok = pAP->getProperty("frame-width", pValue);
    if (ok && pValue != NULL) {
        ODe_writeAttribute(output, "svg:width", pValue);
    }
    
    ok = pAP->getProperty("frame-height", pValue);
    if (ok && pValue != NULL) {
        ODe_writeAttribute(output, "svg:height", pValue);
    }
    
    output += "><draw:image xlink:href=\"Pictures/";
    output += pImageName;
    output += "\" xlink:type=\"simple\" xlink:show=\"embed\""
              " xlink:actuate=\"onLoad\"/>";

    ok = pAP->getAttribute("title", pValue);
    if (ok && pValue != NULL) {
       escape = pValue;
       escape.escapeXML();
       if(escape.length()) {
           output += "<svg:title>";
           output += escape.utf8_str();
           output += "</svg:title>";
       }
    }

    ok = pAP->getAttribute("alt", pValue);
    if (ok && pValue != NULL) {
       escape = pValue;
       escape.escapeXML();
       if(escape.length()) {
           output += "<svg:desc>";
           output += escape.utf8_str();
           output += "</svg:desc>";
       }
       escape.clear();
    }

    output += "</draw:frame></text:p>";
    
    ODe_writeUTF8String(m_pParagraphContent, output);
}


/**
 * Returns true if the properties belongs to a plain paragraph, false otherwise.
 * An AbiWord <p> tag (block) can be, for instance, a list item if it has
 * a "listid" and/or "level" attribute.
 */
bool ODe_Text_Listener::_blockIsPlainParagraph(const PP_AttrProp* pAP) const {
    const gchar* pValue;
    bool ok;
    
    ok = pAP->getAttribute("level", pValue);
    if (ok && pValue != NULL) {
        return false;
    }
    
    ok = pAP->getAttribute("listid", pValue);
    if (ok && pValue != NULL) {
        return false;
    }
    
    return true;
}


/**
 * Open a <text:list-item>, in some cases along with a  preceding <text:list>
 */
void ODe_Text_Listener::_openODListItem(const PP_AttrProp* pAP) {
    int level;
    const gchar* pValue;
    bool ok;
    UT_UTF8String output;

   
    ok = pAP->getAttribute("level", pValue);
    if (ok && pValue != NULL) {
        level = atoi(pValue);
    } else {
        level = 0; // The list will be completely closed.
    }
    

    // This list item may belong to a new list.
    // If so, we must close the current one (if there is a current one at all).
    if (level == 1 && m_currentListLevel > 0) {
        // OBS: An Abi list must start with a level 1 list item.
        
        const ODe_ListLevelStyle* pListLevelStyle;
        pListLevelStyle = m_pCurrentListStyle->getLevelStyle(1);
        
        ok = pAP->getAttribute("listid", pValue);
        UT_ASSERT_HARMLESS(ok && pValue!=NULL);
                
        if (pValue && pListLevelStyle && (strcmp(pListLevelStyle->getAbiListID().utf8_str(), pValue) != 0)) {
            // This list item belongs to a new list.
            _closeODList(); // Close the current list to start a new one later on.
        }
    }


    if (level > m_currentListLevel) {
        // Open a new sub-list


        output.clear();
        _printSpacesOffset(output);
        
        if(m_currentListLevel == 0) {
            // It's a "root" list.
            
            UT_ASSERT(m_pCurrentListStyle == NULL);
            
            m_pCurrentListStyle = m_rAutomatiStyles.addListStyle();
            
            output += "<text:list text:style-name=\"";
            output += ODe_Style_Style::convertStyleToNCName(m_pCurrentListStyle->getName()).escapeXML();
            output += "\">\n";
            
        } else {
            // It's a sub (nested) list, it will inherit the style of its
            // parent (root).
            output += "<text:list>\n";
        }
        
        ODe_writeUTF8String(m_pTextOutput, output);
        
        m_spacesOffset++;
        
        // It's possibly a new list level style.
        // Update our list style with info regarding this level.
        m_pCurrentListStyle->setLevelStyle(level, *pAP);
        
        m_currentListLevel++;
        
    } else if (level < m_currentListLevel) {
        // Close lists until reach the desired list level.
        
        // Levels increase step-by-step but may, nevertheless, decrease
        // many at once.

        // Note that list items are never closed alone. They are aways closed
        // together with a list end tag (</text:list>).
        
        while (m_currentListLevel > level) {
            // Close the current item and its list
            
            output.clear();

            m_spacesOffset--;            
            _printSpacesOffset(output);
            output += "</text:list-item>\n";

            m_spacesOffset--;
            _printSpacesOffset(output);
            output += "</text:list>\n";
            
            ODe_writeUTF8String(m_pTextOutput, output);
            m_currentListLevel--;
        }
        
        
        if (m_currentListLevel > 0) {
            // And, finnaly, close the item that is hold that table hierarchy
            output.clear();
            m_spacesOffset--;
            _printSpacesOffset(output);
            output += "</text:list-item>\n";
            
            ODe_writeUTF8String(m_pTextOutput, output);
        }
        
    } else if (m_currentListLevel > 0) {
        // Same level, just close the current list item.
        output.clear();
        m_spacesOffset--;
        _printSpacesOffset(output);
        output += "</text:list-item>\n";
        
        ODe_writeUTF8String(m_pTextOutput, output);
    }
    
    if (m_currentListLevel  > 0) {
        // Yes, we are inside a list item (so let's create one).
        
        // Note that list items are never closed alone. they are aways closed
        // together with a list end tag (</text:list>).
    
        output.clear();
        _printSpacesOffset(output);
        output += "<text:list-item>\n";
    
        ODe_writeUTF8String(m_pTextOutput, output);
        
        m_spacesOffset++;
    } else {
        m_pCurrentListStyle = NULL;
    }
}


/**
 * 
 */
void ODe_Text_Listener::_openODParagraph(const PP_AttrProp* pAP) {
    ////
    // Figure out the paragraph style
    m_delayedAP = pAP;
    if (ODe_Style_Style::hasParagraphStyleProps(pAP) ||
        ODe_Style_Style::hasTextStyleProps(pAP) ||
        m_pendingMasterPageStyleChange ||
        m_pendingColumnBreak ||
        m_pendingPageBreak) {
            
        // Need to create a new automatic style to hold those paragraph
        // properties.
        
        m_delayedListStyle = m_pCurrentListStyle;
        if (m_pendingMasterPageStyleChange){
                m_delayedPendingMasterPageStyleChange = m_pendingMasterPageStyleChange;
                m_delayedMasterPageStyleName = m_masterPageStyleName.utf8_str();
                m_masterPageStyleName.clear();
                m_pendingMasterPageStyleChange = false;
        }
                
        // Can't have both breaks
        UT_ASSERT(
            !(m_pendingColumnBreak==true && m_pendingPageBreak==true) );
        
        if (m_pendingColumnBreak && !m_bAfter){
            m_delayedColumnBreak = true;
            m_pendingPageBreak = false;
        }
        
        if (m_pendingPageBreak && !m_bAfter){
            m_delayedPageBreak = true;
            m_pendingColumnBreak = false;
        }
        
                 
    } 
    
    ////
    // Write the output string
    m_delayedSpacesOffset = m_spacesOffset;
    m_openedODParagraph = true;
    m_isFirstCharOnParagraph = true;
    m_spacesOffset++;
    
    // The paragraph content will be stored in a separate temp file.
    // It's done that way because we may have to write a textbox (<draw:frame>)
    // inside this paragraph, before its text content, which, in AbiWord, comes
    // before the textbox.
    UT_ASSERT(m_pParagraphContent==NULL);
    m_pParagraphContent = gsf_output_memory_new();
}


/**
 * Close an OpenDocument list if there is one currently open.
 */
void ODe_Text_Listener::_closeODList() {
    if (m_currentListLevel == 0) {
        // There is nothing to be done
        return;
    }
    
    UT_uint8 i;
    UT_UTF8String output;
    
    for (i=m_currentListLevel; i>0; i--) {
        output.clear();
        
        m_spacesOffset--;
        _printSpacesOffset(output);
        output += "</text:list-item>\n";
        
        m_spacesOffset--;
        _printSpacesOffset(output);
        output += "</text:list>\n";
	
        ODe_writeUTF8String(m_pTextOutput, output);
    }
    
    m_currentListLevel = 0;
    m_pCurrentListStyle = NULL;
}


void ODe_Text_Listener::_openParagraphDelayed(){
    UT_UTF8String styleName;
    UT_UTF8String output;
    UT_UTF8String str;
    UT_UTF8String escape;
    const gchar* pValue;
    bool ok;
    
    ////
    // Figure out the paragraph style
    if (m_pendingColumnBreak){
        m_delayedColumnBreak = true;
        m_pendingColumnBreak = false;
    }

    if (m_pendingPageBreak){
        m_delayedPageBreak = true;
        m_pendingPageBreak = false;
    }
    
    if (ODe_Style_Style::hasParagraphStyleProps(m_delayedAP) ||
        ODe_Style_Style::hasTextStyleProps(m_delayedAP) ||
        m_delayedPendingMasterPageStyleChange ||
        m_delayedColumnBreak ||
        m_delayedPageBreak) {
            
        // Need to create a new automatic style to hold those paragraph
        // properties.
        
        ODe_Style_Style* pStyle;
        pStyle = new ODe_Style_Style();
        pStyle->setFamily("paragraph");
        
        pStyle->fetchAttributesFromAbiBlock(m_delayedAP, m_delayedListStyle);
        
        if (m_delayedPendingMasterPageStyleChange) {
            pStyle->setMasterPageName(m_delayedMasterPageStyleName.c_str());
        }
        
        
        if (m_delayedColumnBreak && !m_bAfter) {
            pStyle->setBreakBefore("column");
               m_delayedColumnBreak = false;
        }
        
        if (m_delayedPageBreak && !m_bAfter) {
            pStyle->setBreakBefore("page");
            m_delayedPageBreak = false;
        }
        
        m_rAutomatiStyles.storeParagraphStyle(pStyle);
        styleName = pStyle->getName();

        // There is a special case for the default-tab-interval property, as in
        // AbiWord that is a paragraph property, but in ODF it belongs in the
        // default style for the "paragraph" family.
        ok = m_delayedAP->getProperty("default-tab-interval", pValue);
        if (ok && pValue != NULL) {
            UT_DEBUGMSG(("Got a default tab interval:  !!!!!!!!!!!!! %s\n", pValue));
        }
            
    } else {
        ok = m_delayedAP->getAttribute("style", pValue);
        if (ok) {
            styleName = pValue;
        }
    }
    
    
    ////
    // Write the output string
    UT_uint32 oldOffsets = m_spacesOffset;
    m_spacesOffset = m_delayedSpacesOffset;
    output.clear();
    _printSpacesOffset(output);
    m_spacesOffset = oldOffsets;
    
    if (styleName.empty()) {
        output += "<text:p>";
        ODe_writeUTF8String(m_pParagraphContent, "</text:h>\n");
    } else {
        UT_uint8 outlineLevel = 0;
        
        // Use the original AbiWord style name to see which outline level this
        // style belongs to (if any). Don't use the generated ODT style for this,
        // as that name is not what AbiWord based its decisions on.
        ok = m_delayedAP->getAttribute("style", pValue);
        if (ok) {
            outlineLevel = m_rAuxiliaryData.m_headingStyles.
                            getHeadingOutlineLevel(pValue);
        }
        
        if (outlineLevel > 0) {
            // It's a heading.
            
            UT_UTF8String_sprintf(str, "%u", outlineLevel);
            
            escape = styleName;
            output += "<text:h text:style-name=\"";
            output += ODe_Style_Style::convertStyleToNCName(escape).escapeXML();
            output += "\" text:outline-level=\"";
            output += str;
            output += "\" ";
            const char* xmlid = nullptr;
            if( m_delayedAP->getAttribute( PT_XMLID, xmlid ) && xmlid )
            {
                appendAttribute( output, "xml:id", xmlid );
            }
            output += " >";            
            ODe_writeUTF8String(m_pParagraphContent, "</text:h>\n");
        } else {
            // It's a regular paragraph.
            escape = styleName;
            output += "<text:p text:style-name=\"";
            output += ODe_Style_Style::convertStyleToNCName(escape).escapeXML();
            output += "\" ";
            const char* xmlid = nullptr;
            if( m_delayedAP->getAttribute( PT_XMLID, xmlid ) && xmlid )
            {
                appendAttribute( output, "xml:id", xmlid );
            }
            output += ">";
            ODe_writeUTF8String(m_pParagraphContent, "</text:p>\n");
        }
    }

    ////
    // Write output string to file and update related variables.
    ODe_writeUTF8String(m_pTextOutput, output);
}

/**
 * 
 */
void ODe_Text_Listener::_closeODParagraph() {

    if (m_openedODParagraph) { 
        _openParagraphDelayed();
        gsf_output_write(m_pTextOutput, gsf_output_size(m_pParagraphContent),
			 gsf_output_memory_get_bytes(GSF_OUTPUT_MEMORY(m_pParagraphContent)));

        ODe_gsf_output_close(m_pParagraphContent);
        m_pParagraphContent = NULL;
    
        m_openedODParagraph = false;
        m_spacesOffset--;
        
        m_bAfter = false;
    }
}

UT_UTF8String& ODe_Text_Listener::appendAttribute(
    UT_UTF8String& ret,
    const char* key,
    const char* value )
{
    UT_UTF8String escape = value;
    ret += " ";
    ret += key;
    ret += "=\"";
    ret += escape.escapeXML();
    ret += "\" ";
    return ret;
}

/*
 * Initializing style list with default values to provide correct
 * outline numbers. Doesn't handle numbered headings
 *
 * TODO: Replace this with something more correct
 */
void ODe_Text_Listener::_initDefaultHeadingStyles()
{
	 for (UT_sint32 iLevel = 1; iLevel <= 4; iLevel++) {
	        // gather the source style names for all levels
	        UT_UTF8String sSourceStyle = UT_UTF8String_sprintf("toc-source-style%d", iLevel);
	        const PP_Property* pProp = PP_lookupProperty(sSourceStyle.utf8_str());
	        UT_continue_if_fail(pProp);
	        m_rAuxiliaryData.m_headingStyles.addStyleName(pProp->getInitial(), iLevel);


	        // gather the destination style names for all levels
	        UT_UTF8String sDestStyle = UT_UTF8String_sprintf("toc-dest-style%u", iLevel);
	        UT_UTF8String destStyle;
	        destStyle = fl_TOCLayout::getDefaultDestStyle(iLevel);
	        m_rAuxiliaryData.m_mDestStyles[iLevel] = destStyle;
	        m_rStyles.addStyle(destStyle);
	 }
}
