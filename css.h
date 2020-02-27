const char CSS_page[] PROGMEM = R"=====(

  <meta charset="utf-8">
  
  <title>BeerScale</title>
  <meta name="description" content="Beercooler">
  <meta name="author" content="Ola Wallin/Filip Roos Eriksson">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">

  <style>
    html, body {
      padding: 0px;
      margin: 0px;
      color: hsl(240, 25%, 95%);
      background: #50514F;
        font-family: Verdana, Geneva, sans-serif;
      font-size: 1.1rem;
      font-weight: 400;
    }

    H1, H2, H3, p {
      padding: 0px;
      margin: 0px;
    }

    input[type=button], input[type=submit], input[type=reset] {
        background-color: #A4BAB7;
        border: none;
        color: #2D3332;
        padding: 8px 16px;
        text-decoration: none;
      font-weight: bold;
        margin: 4px 2px;
        cursor: pointer;
    }

    header {
      display: flex;
      justify-content: center;
      flex-direction: column;
      align-items: center;
    }

    main {
      display: flex;
      flex-direction: column;
      justify-content: center;
      align-items: center;
      padding-top: 15px;
      padding-bottom: 15px;
    }

    footer {
      display: flex;
      justify-content: space-evenly;
    }
    form {
      display: flex;
      flex-direction: column;
    }
  </style>

</head>



)=====";
