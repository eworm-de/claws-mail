<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:fo="http://www.w3.org/1999/XSL/Format"
                version="1.0">

<xsl:import href="@DOCBOOK_XSL_PATH@/fo/docbook.xsl"/>

<xsl:param name="fop1.extensions" select="1"/>
<xsl:param name="title.margin.left" select="'0pt'"/>
<xsl:param name="body.start.indent" select="'0pt'"/>
<xsl:param name="variablelist.as.blocks" select="1"/>
<xsl:param name="paper.type" select="'A4'"/>

<xsl:template match="processing-instruction('hard-pagebreak')">
   <fo:block break-before='page'/>
</xsl:template>
 
<xsl:template match="appendix[@id='ap_glossary']/variablelist"
              mode="fop1.outline">

  <xsl:variable name="id">
    <xsl:call-template name="object.id"/>
  </xsl:variable>
  
  <xsl:variable name="bookmark-label">
    <xsl:apply-templates select="." mode="object.title.markup"/>
  </xsl:variable>

  <!-- Put the root element bookmark at the same level as its children -->
  <!-- If the object is a set or book, generate a bookmark for the toc -->

  <xsl:choose>
    <xsl:when test="parent::*">
      <fo:bookmark internal-destination="{$id}">
        <fo:bookmark-title>
          <xsl:value-of select="normalize-space(translate($bookmark-label, $a-dia, $a-asc))"/>
        </fo:bookmark-title>
        <xsl:apply-templates select="*" mode="fop1.outline"/>
      </fo:bookmark>
    </xsl:when>
    <xsl:otherwise>
      <fo:bookmark internal-destination="{$id}">
        <fo:bookmark-title>
          <xsl:value-of select="normalize-space(translate($bookmark-label, $a-dia, $a-asc))"/>
        </fo:bookmark-title>
      </fo:bookmark>

      <xsl:variable name="toc.params">
        <xsl:call-template name="find.path.params">
          <xsl:with-param name="table" select="normalize-space($generate.toc)"/>
        </xsl:call-template>
      </xsl:variable>

      <xsl:if test="contains($toc.params, 'toc')
                    and (book|part|reference|preface|chapter|appendix|article
                         |glossary|bibliography|index|setindex
                         |refentry
                         |sect1|sect2|sect3|sect4|sect5|section)">
        <fo:bookmark internal-destination="toc...{$id}">
          <fo:bookmark-title>
            <xsl:call-template name="gentext">
              <xsl:with-param name="key" select="'TableofContents'"/>
            </xsl:call-template>
          </fo:bookmark-title>
        </fo:bookmark>
      </xsl:if>
      <xsl:apply-templates select="*" mode="fop1.outline"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

</xsl:stylesheet>
