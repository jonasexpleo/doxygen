/******************************************************************************
 *
 * 
 *
 * Copyright (C) 1997-2001 by Dimitri van Heesch.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation under the terms of the GNU General Public License is hereby 
 * granted. No representations are made about the suitability of this software 
 * for any purpose. It is provided "as is" without express or implied warranty.
 * See the GNU General Public License for more details.
 *
 * Documents produced by Doxygen are derivative works derived from the
 * input used in their production; they are not affected by this license.
 *
 */

#include "qtbc.h"
#include "memberlist.h"
#include "classlist.h"
#include "filedef.h"
#include "doc.h"
#include "doxygen.h"
#include "memberdef.h"
#include "classdef.h"
#include "namespacedef.h"
#include "util.h"
#include "language.h"
#include "outputlist.h"
#include "dot.h"
#include "message.h"
#include "code.h"
//#include "xml.h"

/*! create a new file definition, where \a p is the file path, 
    \a the file name, and \a ref is an HTML anchor name if the
    file was read from a tag file or 0 otherwise
*/
FileDef::FileDef(const char *p,const char *nm,const char *lref)
   : Definition((QCString)p+nm,1,nm)
{
  path=p;
  filepath=path+nm;
  filename=nm;
  diskname=nm;
  setReference(lref);
  classList     = new ClassList;
  includeList   = new QList<IncludeInfo>;
  includeList->setAutoDelete(TRUE);
  includeDict   = new QDict<IncludeInfo>(61);
  includedByList = new QList<IncludeInfo>;
  includedByList->setAutoDelete(TRUE);
  includedByDict = new QDict<IncludeInfo>(61);
  namespaceList = new NamespaceList;
  namespaceDict = new NamespaceDict(7);
  srcDefDict = 0;
  srcMemberDict = 0;
  usingDirList = 0;
  usingDeclList = 0;
  package = 0;
  isSource = FALSE; 
  docname = nm;
  if (Config_getBool("FULL_PATH_NAMES"))
  {
    docname.prepend(stripFromPath(path.copy()));
  }
  memberGroupList = new MemberGroupList;
  memberGroupList->setAutoDelete(TRUE);
  memberGroupDict = new MemberGroupDict(1009);
}

/*! destroy the file definition */
FileDef::~FileDef()
{
  delete classList;
  delete includeDict;
  delete includeList;
  delete namespaceList;
  delete namespaceDict;
  delete srcDefDict;
  delete srcMemberDict;
  delete usingDirList;
  delete usingDeclList;
  delete memberGroupList;
  delete memberGroupDict;
}

/*! Compute the HTML anchor names for all members in the class */ 
void FileDef::computeAnchors()
{
  setAnchors('a',&allMemberList);
}

void FileDef::distributeMemberGroupDocumentation()
{
  MemberGroupListIterator mgli(*memberGroupList);
  MemberGroup *mg;
  for (;(mg=mgli.current());++mgli)
  {
    mg->distributeMemberGroupDocumentation();
  }
}

/*! Write the documentation page for this file to the file of output
    generators \a ol. 
*/
void FileDef::writeDocumentation(OutputList &ol)
{
  //funcList->countDecMembers();
  
  //QCString fn = name();
  //if (Config_getBool("FULL_PATH_NAMES"))
  //{
  //  fn.prepend(stripFromPath(getPath().copy()));
  //}

  //printf("WriteDocumentation %p diskname=%s\n",this,diskname.data());
  
  QCString pageTitle=name()+" File Reference";
  startFile(ol,getOutputFileBase(),pageTitle);
  startTitle(ol,getOutputFileBase());
  parseText(ol,theTranslator->trFileReference(docname));
  endTitle(ol,getOutputFileBase(),docName());
  //ol.newParagraph();
  
  if (!Config_getString("GENERATE_TAGFILE").isEmpty()) 
  {
    Doxygen::tagFile << "  <compound kind=\"file\">" << endl;
    Doxygen::tagFile << "    <name>" << convertToXML(name()) << "</name>" << endl;
    Doxygen::tagFile << "    <path>" << convertToXML(getPath()) << "</path>" << endl;
    Doxygen::tagFile << "    <filename>" << convertToXML(getOutputFileBase()) << ".html</filename>" << endl;
  }
  
  ol.startTextBlock();
  //brief=brief.stripWhiteSpace();
  //int bl=brief.length();
  OutputList briefOutput(&ol); 
  if (briefDescription()) 
  {
    //if (brief.at(bl-1)!='.' && brief.at(bl-1)!='!' && brief.at(bl!='?')) 
    //  brief+='.';
    parseDoc(briefOutput,filepath,1,0,0,briefDescription());
    ol+=briefOutput;
    ol.writeString(" \n");
    ol.disableAllBut(OutputGenerator::Html);
    ol.startTextLink(0,"_details");
    parseText(ol,theTranslator->trMore());
    ol.endTextLink();
    ol.enableAll();
    ol.disable(OutputGenerator::Man);
    ol.newParagraph();
    ol.enable(OutputGenerator::Man);
  }
  ol.writeSynopsis();
 
  if (Config_getBool("SHOW_INCLUDE_FILES"))
  {
    ol.startTextBlock(TRUE);
    QListIterator<IncludeInfo> ili(*includeList);
    IncludeInfo *ii;
    for (;(ii=ili.current());++ili)
    {
      FileDef *fd=ii->fileDef;
      ol.startTypewriter();
      ol.docify("#include ");
      if (ii->local)
        ol.docify("\"");
      else
        ol.docify("<");
      ol.disable(OutputGenerator::Html);
      ol.docify(ii->includeName);
      ol.enableAll();
      ol.disableAllBut(OutputGenerator::Html);
      
      // Here we use the include file name as it appears in the file.
      // we could also we the name as it is used within doxygen,
      // then we should have used fd->docName() instead of ii->includeName
      if (fd && fd->isLinkable() && fd->generateSourceFile())
      {
        ol.writeObjectLink(fd->getReference(),fd->includeName(),0,ii->includeName);
      }
      else
      {
        ol.docify(ii->includeName);
      }
      
      ol.enableAll();
      if (ii->local)
        ol.docify("\"");
      else
        ol.docify(">");
      ol.endTypewriter();
      ol.disable(OutputGenerator::RTF);
      ol.lineBreak();
      ol.enableAll();
      ol.disableAllBut(OutputGenerator::RTF);
      ol.newParagraph();
      ol.enableAll();
    }
    ol.endTextBlock();
  }
  
  if (Config_getBool("HAVE_DOT") && Config_getBool("INCLUDE_GRAPH"))
  {
    //printf("Graph for file %s\n",name().data());
    DotInclDepGraph incDepGraph(this,FALSE);
    if (!incDepGraph.isTrivial())
    {
      ol.disable(OutputGenerator::Man);
      ol.newParagraph();
      ol.startInclDepGraph();
      parseText(ol,theTranslator->trInclDepGraph(name()));
      ol.endInclDepGraph(incDepGraph);
      ol.enableAll();
    }
    //incDepGraph.writeGraph(Config_getString("HTML_OUTPUT"),fd->getOutputFileBase());
  }

  if (Config_getBool("HAVE_DOT") && Config_getBool("INCLUDED_BY_GRAPH"))
  {
    //printf("Graph for file %s\n",name().data());
    DotInclDepGraph incDepGraph(this,TRUE);
    if (!incDepGraph.isTrivial())
    {
      ol.disable(OutputGenerator::Man);
      ol.newParagraph();
      ol.startInclDepGraph();
      parseText(ol,theTranslator->trInclByDepGraph());
      ol.endInclDepGraph(incDepGraph);
      ol.enableAll();
    }
    //incDepGraph.writeGraph(Config_getString("HTML_OUTPUT"),fd->getOutputFileBase());
  }

  //printf("%s: generateSourceFile()=%d\n",name().data(),generateSourceFile());
  if (generateSourceFile())
  {
    ol.disableAllBut(OutputGenerator::Html);
    ol.newParagraph();
    ol.startTextLink(includeName(),0);
    parseText(ol,theTranslator->trGotoSourceCode());
    ol.endTextLink();
    ol.enableAll();
  }
  
  //ol.disableAllBut(OutputGenerator::Html);
  //ol.writeString((QCString)"<p>Interface collaboration diagram for "
  //               "<a href=\"usage_intf_graph_"+name()+".html\">here</a>");
  //ol.writeString((QCString)"<p>Include dependency diagram for "+fn+" can be found "+
  //               "<a href=\""+diskname+"_incldep.html\">here</a>.");
  //ol.enableAll();

  ol.endTextBlock();
  
  ol.startMemberSections();

  if (namespaceList->count()>0)
  {
    NamespaceDef *nd=namespaceList->first();
    bool found=FALSE;
    while (nd)
    {
      if (nd->name().find('@')==-1)
      {
        if (!found)
        {
          ol.startMemberHeader();
          parseText(ol,theTranslator->trNamespaces());
          ol.endMemberHeader();
          ol.startMemberList();
          found=TRUE;
        }
        ol.startMemberItem(0);
        ol.writeString("namespace ");
        ol.insertMemberAlign();
        if (nd->isLinkable()) 
        {
          ol.writeObjectLink(nd->getReference(),
              nd->getOutputFileBase(),
              0,
              nd->name()
          );
          if (!Config_getString("GENERATE_TAGFILE").isEmpty()) 
          {
            Doxygen::tagFile << "    <namespace>" << convertToXML(nd->name()) << "</namespace>" << endl;
          }
        }
        else
        {
          ol.startBold();
          ol.docify(nd->name());
          ol.endBold();
        }
        ol.endMemberItem(FALSE);
      }
      nd=namespaceList->next();
    }
    if (found) ol.endMemberList();
  }

  classList->writeDeclaration(ol);
  
  /* write user defined member groups */
  MemberGroupListIterator mgli(*memberGroupList);
  MemberGroup *mg;
  for (;(mg=mgli.current());++mgli)
  {
    if (mg->header()!="[NOHEADER]")
    {
      mg->writeDeclarations(ol,0,0,this,0);
    }
  }

  allMemberList.writeDeclarations(ol,0,0,this,0,0,0);
  ol.endMemberSections();

  //doc=doc.stripWhiteSpace();
  //int bl=brief.length();
  //int dl=doc.length();
  if ((!briefDescription().isEmpty() && Config_getBool("REPEAT_BRIEF")) || 
      !documentation().isEmpty() 
      /* || startBodyLine!=-1 */
     )
  {
    ol.writeRuler();
    ol.pushGeneratorState();
    //bool latexOn = ol.isEnabled(OutputGenerator::Latex);
    //if (latexOn) ol.disable(OutputGenerator::Latex);
    ol.disable(OutputGenerator::Latex);
    ol.disable(OutputGenerator::RTF);
    ol.writeAnchor(0,"_details"); 
    //if (latexOn) ol.enable(OutputGenerator::Latex);
    ol.popGeneratorState();
    ol.startGroupHeader();
    parseText(ol,theTranslator->trDetailedDescription());
    ol.endGroupHeader();
    if (!briefDescription().isEmpty() && Config_getBool("REPEAT_BRIEF"))
    {
      ol+=briefOutput;
    }
    if (!briefDescription().isEmpty() && Config_getBool("REPEAT_BRIEF") && 
        !documentation().isEmpty())
    {
      ol.newParagraph();
    }
    if (!documentation().isEmpty())
    {
      //if (doc.at(dl-1)!='.' && doc.at(dl-1)!='!' && doc.at(dl-1)!='?') 
      //  doc+='.';
      parseDoc(ol,filepath,1,0,0,documentation()+"\n");
    }
    //printf("Writing source ref for file %s\n",name().data());
    if (Config_getBool("SOURCE_BROWSER")) 
    {
      ol.newParagraph();
      QCString refText = theTranslator->trDefinedInSourceFile();
      int fileMarkerPos = refText.find("@0");
      if (fileMarkerPos!=-1) // should always pass this.
      {
        parseText(ol,refText.left(fileMarkerPos)); //text left from marker 1
        ol.writeObjectLink(0,getSourceFileBase(),
            0,name());
        parseText(ol,refText.right(
              refText.length()-fileMarkerPos-2)); // text right from marker 2
      }
    }
  }

  defineMembers.countDocMembers();
  if (defineMembers.totalCount()>0 )
  {
    ol.writeRuler();
    ol.startGroupHeader();
    parseText(ol,theTranslator->trDefineDocumentation());
    ol.endGroupHeader();
    defineMembers.writeDocumentation(ol,name(),this);
  }
  
  protoMembers.countDocMembers(); 
  if (protoMembers.totalCount()>0 )
  {
    ol.writeRuler();
    ol.startGroupHeader();
    parseText(ol,theTranslator->trFunctionPrototypeDocumentation());
    ol.endGroupHeader();
    protoMembers.writeDocumentation(ol,name(),this);
  }

  typedefMembers.countDocMembers();
  if (typedefMembers.totalCount()>0 )
  {
    ol.writeRuler();
    ol.startGroupHeader();
    parseText(ol,theTranslator->trTypedefDocumentation());
    ol.endGroupHeader();
    typedefMembers.writeDocumentation(ol,name(),this);
  }
  
  enumMembers.countDocMembers();
  if (enumMembers.totalCount()>0 )
  {
    ol.writeRuler();
    ol.startGroupHeader();
    parseText(ol,theTranslator->trEnumerationTypeDocumentation());
    ol.endGroupHeader();
    enumMembers.writeDocumentation(ol,name(),this);
  }

  funcMembers.countDocMembers();
  if (funcMembers.totalCount()>0 )
  {
    ol.writeRuler();
    ol.startGroupHeader();
    parseText(ol,theTranslator->trFunctionDocumentation());
    ol.endGroupHeader();
    funcMembers.writeDocumentation(ol,name(),this);
  }
  
  varMembers.countDocMembers();
  if (varMembers.totalCount()>0 )
  {
    ol.writeRuler();
    ol.startGroupHeader();
    parseText(ol,theTranslator->trVariableDocumentation());
    ol.endGroupHeader();
    varMembers.writeDocumentation(ol,name(),this);
  }
  
  // write Author section (Man only)
  ol.disableAllBut(OutputGenerator::Man);
  ol.startGroupHeader();
  parseText(ol,theTranslator->trAuthor());
  ol.endGroupHeader();
  parseText(ol,theTranslator->trGeneratedAutomatically(Config_getString("PROJECT_NAME")));
  ol.enableAll();

  if (!Config_getString("GENERATE_TAGFILE").isEmpty()) 
  {
    writeDocAnchorsToTagFile();
    Doxygen::tagFile << "  </compound>" << endl;
  }

  endFile(ol);
}

/*! Write a source listing of this file to the output */
void FileDef::writeSource(OutputList &ol)
{
  ol.disableAllBut(OutputGenerator::Html);
  startFile(ol,getSourceFileBase(),docname+" Source File");
  startTitle(ol,0);
  parseText(ol,docname);
  endTitle(ol,0,0);

  if (isLinkable())
  {
    ol.startTextLink(getOutputFileBase(),0);
    parseText(ol,theTranslator->trGotoDocumentation());
    ol.endTextLink();
  }

  initParseCodeContext();
  ol.startCodeFragment();
  parseCode(ol,0,
            fileToString(absFilePath(),Config_getBool("FILTER_SOURCE_FILES")),
            FALSE,0,this
           );
  ol.endCodeFragment();
  endFile(ol);
  ol.enableAll();
}

/*! Adds a member \a md to the member group with id \a groupId.
 */ 
void FileDef::addMemberListToGroup(MemberList *ml,
                               bool (MemberDef::*func)() const)
{
  MemberListIterator mli(*ml);
  MemberDef *md;
  for (;(md=mli.current());++mli)
  {
    int groupId=md->getMemberGroupId();
    if ((md->*func)() && groupId!=-1)
    {
      QCString *pGrpHeader = Doxygen::memberHeaderDict[groupId];
      QCString *pDocs      = Doxygen::memberDocDict[groupId];
      //printf("Member `%s' pGrpHeader=%p\n",md->name().data(),pGrpHeader);
      if (pGrpHeader)
      {
        MemberGroup *mg = memberGroupDict->find(groupId);
        if (mg==0)
        {
          mg = new MemberGroup(groupId,*pGrpHeader,pDocs ? pDocs->data() : 0);
          memberGroupDict->insert(groupId,mg);
          memberGroupList->append(mg);
        }
        //printf("insert member %s in group %s\n",md->name().data(),pGrpHeader->data());
        mg->insertMember(md);
        md->setMemberGroup(mg);
      }
    }
  }
}

void FileDef::addMembersToMemberGroup()
{
  addMemberListToGroup(&allMemberList,&MemberDef::isDefine);
  addMemberListToGroup(&allMemberList,&MemberDef::isTypedef);
  addMemberListToGroup(&allMemberList,&MemberDef::isEnumerate);
  addMemberListToGroup(&allMemberList,&MemberDef::isEnumValue);
  addMemberListToGroup(&allMemberList,&MemberDef::isFunction);
  addMemberListToGroup(&allMemberList,&MemberDef::isVariable);
}

/*! Adds member definition \a md to the list of all members of this file */
void FileDef::insertMember(MemberDef *md)
{
  //printf("%s:FileDef::insertMember(%s)\n",name().data(),md->name().data());
  allMemberList.append(md); 
  bool sortMemberDocs = Config_getBool("SORT_MEMBER_DOCS");
  switch(md->memberType())
  {
    case MemberDef::Variable:     
      if (sortMemberDocs)
        varMembers.inSort(md); 
      else
        varMembers.append(md);
      break;
    case MemberDef::Function: 
      if (sortMemberDocs)    
        funcMembers.inSort(md); 
      else
        funcMembers.append(md);
      break;
    case MemberDef::Typedef:      
      if (sortMemberDocs)
        typedefMembers.inSort(md); 
      else
        typedefMembers.append(md);
      break;
    case MemberDef::Enumeration:  
      if (sortMemberDocs)
        enumMembers.inSort(md); 
      else
        enumMembers.append(md);
      break;
    case MemberDef::EnumValue:    // enum values are shown inside their enums
      break;
    case MemberDef::Prototype:    
      if (sortMemberDocs)
        protoMembers.inSort(md); 
      else
        protoMembers.append(md);
      break;
    case MemberDef::Define:       
      if (sortMemberDocs)
        defineMembers.inSort(md); 
      else
        defineMembers.append(md);
      break;
    default:
       err("FileDef::insertMembers(): unexpected member insert in file!\n");
  }
  //addMemberToGroup(md,groupId);
}

/*! Adds compound definition \a cd to the list of all compounds of this file */
void FileDef::insertClass(ClassDef *cd)
{
  if (Config_getBool("SORT_MEMBER_DOCS"))
    classList->inSort(cd);
  else
    classList->append(cd);
}

/*! Adds namespace definition \a nd to the list of all compounds of this file */
void FileDef::insertNamespace(NamespaceDef *nd)
{
  if (!nd->name().isEmpty() && namespaceDict->find(nd->name())==0)
  {
    if (Config_getBool("SORT_MEMBER_DOCS"))
      namespaceList->inSort(nd);
    else
      namespaceList->append(nd);
    namespaceDict->insert(nd->name(),nd);
  }
}

void FileDef::addSourceRef(int line,Definition *d,MemberDef *md)
{
  if (d)
  {
    if (srcDefDict==0)    srcDefDict    = new QIntDict<Definition>(257);
    if (srcMemberDict==0) srcMemberDict = new QIntDict<MemberDef>(257);
    srcDefDict->insert(line,d);
    if (md) srcMemberDict->insert(line,md);
    //printf("Adding member %s with anchor %s at line %d to file %s\n",
    //    d->name().data(),anchor,line,name().data());
  }
}

Definition *FileDef::getSourceDefinition(int lineNr)
{
  Definition *result=0;
  if (srcDefDict)
  {
    result = srcDefDict->find(lineNr);
  }
  return result;
}

MemberDef *FileDef::getSourceMember(int lineNr)
{
  MemberDef *result=0;
  if (srcMemberDict)
  {
    result = srcMemberDict->find(lineNr);
  }
  return result;
}

void FileDef::addUsingDirective(NamespaceDef *nd)
{
  if (usingDirList==0)
  {
    usingDirList = new NamespaceList;
  }
  usingDirList->append(nd);
}

void FileDef::addUsingDeclaration(ClassDef *cd)
{
  if (usingDeclList==0)
  {
    usingDeclList = new ClassList;
  }
  usingDeclList->append(cd);
}

void FileDef::addIncludeDependency(FileDef *fd,const char *incName,bool local)
{
  //printf("FileDef::addIncludeDependency(%p,%s,%d)\n",fd,incName,local);
  QCString iName = fd ? fd->absFilePath().data() : incName;
  if (!iName.isEmpty() && includeDict->find(iName)==0)
  {
    IncludeInfo *ii = new IncludeInfo;
    ii->fileDef     = fd;
    ii->includeName = incName;
    ii->local       = local;
    includeList->append(ii);
    includeDict->insert(iName,ii);
  }
}

void FileDef::addIncludedByDependency(FileDef *fd,const char *incName,bool local)
{
  //printf("FileDef::addIncludedByDependency(%p,%s,%d)\n",fd,incName,local);
  QCString iName = fd ? fd->absFilePath().data() : incName;
  if (!iName.isEmpty() && includedByDict->find(iName)==0)
  {
    IncludeInfo *ii = new IncludeInfo;
    ii->fileDef     = fd;
    ii->includeName = incName;
    ii->local       = local;
    includedByList->append(ii);
    includedByDict->insert(iName,ii);
  }
}

bool FileDef::generateSourceFile() const 
{ 
  QCString extension = name().right(4);
  return !isReference() && 
         (Config_getBool("SOURCE_BROWSER") || 
           (Config_getBool("VERBATIM_HEADERS") && guessSection(name())==Entry::HEADER_SEC) 
         ) &&
         extension!=".doc" && extension!=".txt" && extension!=".dox"; 
}

