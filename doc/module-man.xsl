<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                xmlns:ctrlproxy="http://ctrlproxy.vernstok.nl/common"
				version="1.1"
                extension-element-prefixes="exsl">

<xsl:output method="xml"/>

<xsl:param name="xmlCtrlproxyNsUri" select="'http://ctrlproxy.vernstok.nl/common'"/>
<xsl:variable name="secnum" select="'7ctrlproxy'"/>

<xsl:template match="networks"/>

<xsl:template match="ctrlproxy:module">
	<xsl:text disable-output-escaping="yes">
&lt;!DOCTYPE refentry PUBLIC "-//OASIS//DTD DocBook XML V4.2//EN"
		"http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd"&gt;
</xsl:text>
	
	<xsl:element name="refentry">
		<xsl:attribute name="id">
			<xsl:value-of select="@name"/><xsl:text>.</xsl:text><xsl:value-of select="$secnum"/>
		</xsl:attribute>

		<xsl:element name="refmeta">
			<xsl:element name="refentrytitle"><xsl:value-of select="@name"/></xsl:element>
			<xsl:element name="manvolnum"><xsl:value-of select="$secnum"/></xsl:element>
		</xsl:element>

		<xsl:element name="refnamediv">
			<xsl:element name="refname"><xsl:value-of select="@name"/></xsl:element>
			<xsl:element name="refpurpose"><xsl:value-of select="modulemeta/description"/></xsl:element>
		</xsl:element>

		<xsl:apply-templates/>

		<xsl:element name="refsect1">
			<xsl:element name="title">VERSION</xsl:element>

			<xsl:element name="para"><xsl:text>This man page is valid for version </xsl:text><xsl:value-of select="modulemeta/version"/><xsl:text> of the plugin.</xsl:text></xsl:element>
		</xsl:element>

		<xsl:element name="refsect1">
			<xsl:element name="title">SEE ALSO</xsl:element>

			<xsl:element name="para">
				<xsl:text>ctrlproxyrc(5), ctrlproxy(1), </xsl:text>
				<xsl:element name="ulink"><xsl:attribute name="url"><xsl:text>http://ctrlproxy.vernstok.nl/</xsl:text></xsl:attribute><xsl:text>http://ctrlproxy.vernstok.nl/</xsl:text></xsl:element>
				<xsl:if test="modulemeta/homepage != '' and modulemeta/homepage != 'http://ctrlproxy.vernstok.nl/'">
					<xsl:text>, </xsl:text><xsl:element name="ulink"><xsl:attribute name="url"><xsl:value-of select="modulemeta/homepage"/></xsl:attribute><xsl:value-of select="modulemeta/homepage"/></xsl:element>
				</xsl:if>
			</xsl:element>
		</xsl:element>

		<xsl:if test="modulemeta/author != ''">
			<xsl:element name="refsect1">
				<xsl:element name="title">AUTHOR</xsl:element>
			
				<xsl:element name="para">
					<xsl:element name="ulink"><xsl:attribute name="url"><xsl:text>mailto:</xsl:text><xsl:value-of select="@modulemeta/author/email"/></xsl:attribute><xsl:value-of select="modulemeta/author"/></xsl:element>
				</xsl:element>
			</xsl:element>
		</xsl:if>

		<xsl:if test="modulemeta/requirements != ''">
			<xsl:element name="refsect1">
				<xsl:element name="title">REQUIREMENTS</xsl:element>

				<xsl:element name="para">
					<xsl:value-of select="modulemeta/requirements"/>
				</xsl:element>
			</xsl:element>
		</xsl:if>
	</xsl:element>
</xsl:template>

<xsl:template match="element">
	<xsl:element name="varlistentry">
		<xsl:element name="term"><xsl:element name="emphasis"><xsl:value-of select="@name"/></xsl:element></xsl:element>
		<xsl:element name="listitem">
			<xsl:element name="para">
				<xsl:value-of select="description"/>
			</xsl:element>
			<xsl:if test="element != '' or attribute != ''">
				<xsl:element name="variablelist">
					<xsl:apply-templates/>
				</xsl:element>
			</xsl:if>
		</xsl:element>
	</xsl:element>
</xsl:template>

<xsl:template match="ctrlproxy:module/modulemeta"></xsl:template>

<xsl:template match="element/description"></xsl:template>

<xsl:template match="transports"/>

<xsl:template match="attribute">
	<xsl:element name="varlistentry">
		<xsl:element name="term"><xsl:element name="emphasis"><xsl:value-of select="@name"/></xsl:element></xsl:element>
		<xsl:element name="listitem">
			<xsl:element name="para">
				<xsl:value-of select="description"/>
			</xsl:element>
		</xsl:element>
	</xsl:element>
</xsl:template>

<xsl:template match="configuration">
	<xsl:element name="refsect1">
		<xsl:element name="title"><xsl:text>CONFIGURATION</xsl:text></xsl:element>

		<xsl:element name="para"><xsl:text>The following XML elements are supported:</xsl:text></xsl:element>

		<xsl:if test="element != '' or attribute != ''">
			<xsl:element name="variablelist">
				<xsl:apply-templates/>
			</xsl:element>
		</xsl:if>
	</xsl:element>
</xsl:template>

<xsl:template match="ctrlproxy:module/description">
	<xsl:element name="refsect1">
		<xsl:element name="title"><xsl:text>DESCRIPTION</xsl:text></xsl:element>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="ctrlproxy:module/example">
	<xsl:element name="refsect1">
		<xsl:element name="title"><xsl:text>EXAMPLE</xsl:text></xsl:element>
		<xsl:element name="programlisting">
			<xsl:apply-templates/>
		</xsl:element>
	</xsl:element>
</xsl:template>

<xsl:template match="ctrlproxy:module/title"/>

<xsl:template match="todo">
	<xsl:element name="refsect1">
		<xsl:element name="title"><xsl:text>TODO</xsl:text></xsl:element>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="bugs">
	<xsl:element name="refsect1">
		<xsl:element name="title"><xsl:text>BUGS</xsl:text></xsl:element>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="section">
	<xsl:element name="refsect1">
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="subsection">
	<xsl:element name="refsect2">
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<!-- Copy content unchanged -->
<xsl:template match="@*|node()">
	<xsl:copy>
		<xsl:apply-templates select="@*|node()"/>
	</xsl:copy>
</xsl:template>

</xsl:stylesheet>
