<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                xmlns:ctrlproxy="http://ctrlproxy.vernstok.nl/common"
				version="1.1"
                extension-element-prefixes="exsl">

<xsl:output method="xml"/>

    <xsl:template match="ctrlproxy:module">
        <xsl:element name="varlistentry">
            <xsl:element name="term">
                <xsl:value-of select="@name"/>
            </xsl:element>
            <xsl:element name="listitem">
                <xsl:element name="para">
                    <xsl:value-of select="modulemeta/description"/>
                </xsl:element>
            </xsl:element>
        </xsl:element>
	</xsl:template>

	<!-- Copy content unchanged -->
	<xsl:template match="@*|node()">
		<xsl:copy>
			<xsl:apply-templates select="@*|node()"/>
		</xsl:copy>
	</xsl:template>

</xsl:stylesheet>
