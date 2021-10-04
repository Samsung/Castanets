describe('offload worker page', () => {
  before(() => {
    cy.visit('https://localhost:5443/offload-worker.html');
    // FIXME: When calling visit, the page does not work properly, so reload is called.
    cy.reload();
  });

  it('Make sure the page is connecting properly ', () => {
    cy.get('#serverURL').should('exist');
    cy.get('#connectBtn').should('exist');
    cy.get('#logClearBtn').should('exist');
    cy.get('#offloadLogText').should('exist');
  });

  it('Check if connection is made automatically ', () => {
    cy.get('#offloadLogText')
      .find('span')
      .should($span => {
        expect($span).to.have.length(3);
        const connectedText = $span[1].innerText;
        expect(connectedText).to.match(/connected/);
      });
  });

  it('Check connect button operation', () => {
    cy.get('#connectBtn').click();
    cy.get('#offloadLogText')
      .find('span')
      .should($span => {
        expect($span).to.have.length(7);
        const disconnectText = $span[3].innerText;
        expect(disconnectText).to.match(/disconnect/);
        const connectedText = $span[5].innerText;
        expect(connectedText).to.match(/connected/);
      });
  });

  it('Check clear button operation', () => {
    cy.get('#logClearBtn').click();
    cy.get('#offloadLogText')
      .find('span')
      .should($span => {
        expect($span).to.have.length(0);
      });
  });
});
