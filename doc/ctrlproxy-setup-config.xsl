<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:exsl="http://exslt.org/common"
	xmlns:ctrlproxy="http://ctrlproxy.vernstok.nl/common"
	version="1.1"
	extension-element-prefixes="exsl">

	<xsl:output method="text"/>

	<xsl:template match="ctrlproxy:module">
		<xsl:text>'</xsl:text><xsl:value-of select="@name"/><xsl:text>' => {&#10;</xsl:text>
		<xsl:text>autoload => 1, &#10;</xsl:text>
		<xsl:text>desc => "</xsl:text><xsl:value-of select="modulemeta/description"/><xsl:text>",&#10;</xsl:text>
		<xsl:text>options => {&#10;</xsl:text>
		<xsl:for-each select="configuration/element">
			<xsl:call-template name="element"/>
		</xsl:for-each>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text>},&#10;</xsl:text>
	</xsl:template>

	<xsl:template name="element">
		<xsl:value-of select="@name"/><xsl:text> => {&#10;</xsl:text>
		<xsl:if test="@default != ''">
			<xsl:text>default => "</xsl:text><xsl:value-of select="@default"/><xsl:text>",&#10;</xsl:text>
		</xsl:if>

		<xsl:text>desc => "</xsl:text><xsl:value-of select="description"/><xsl:text>",&#10;</xsl:text>

		<xsl:text>type => "</xsl:text>
		<xsl:choose>
			<xsl:when test="@type != ''">
				<xsl:value-of select="@type"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>text</xsl:text>
			</xsl:otherwise>
		</xsl:choose><xsl:text>",&#10;</xsl:text>

		<xsl:text>parameters => {&#10;</xsl:text>
		<xsl:for-each select="attribute">
			<xsl:value-of select="@name"/>
			<xsl:text> => {&#10;</xsl:text>
			<xsl:if test="@default != ''">
				<xsl:text>default => "</xsl:text><xsl:value-of select="@default"/><xsl:text>",&#10;</xsl:text>
			</xsl:if>

			<xsl:text>type => "</xsl:text>
			<xsl:choose>
				<xsl:when test="@type != ''">
					<xsl:value-of select="@type"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>text</xsl:text>
				</xsl:otherwise>
			</xsl:choose><xsl:text>",&#10;</xsl:text>		
			<xsl:text>desc => "</xsl:text><xsl:value-of select="description"/><xsl:text>",&#10;</xsl:text>
			<xsl:text>},&#10;</xsl:text>
		</xsl:for-each>
		<xsl:text>},&#10;</xsl:text>

	

		<xsl:text>options => {&#10;</xsl:text>
		<xsl:for-each select="element">
			<xsl:call-template name="element"/>
		</xsl:for-each>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text>},&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="node()"/>

</xsl:stylesheet>
